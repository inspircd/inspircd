/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *               <omster@gmail.com>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <string>

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "helperfuncs.h"
#include "m_sqlv2.h"
#include "m_sqlutils.h"

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table */

class ModuleSQLAuth : public Module
{
	InspIRCd* Srv;
	Module* SQLutils;

	std::string usertable;
	std::string userfield;
	std::string passfield;
	std::string encryption;
	std::string killreason;
	std::string allowpattern;
	std::string databaseid;
	
	bool verbose;
	
public:
	ModuleSQLAuth(InspIRCd* Me)
	: Module::Module(Me), Srv(Me)
	{
		SQLutils = Srv->FindFeature("SQLutils");
		
		if(SQLutils)
		{
			ServerInstance->Log(DEBUG, "Successfully got SQLutils pointer");
		}
		else
		{
			ServerInstance->Log(DEFAULT, "ERROR: This module requires a module offering the 'SQLutils' feature (usually m_sqlutils.so). Please load it and try again.");
			throw ModuleException("This module requires a module offering the 'SQLutils' feature (usually m_sqlutils.so). Please load it and try again.");
		}
				
		OnRehash("");
	}

	void Implements(char* List)
	{
		List[I_OnUserDisconnect] = List[I_OnCheckReady] = List[I_OnRequest] = List[I_OnRehash] = List[I_OnUserRegister] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		ConfigReader Conf(Srv);
		
		usertable	= Conf.ReadValue("sqlauth", "usertable", 0);	/* User table name */
		databaseid	= Conf.ReadValue("sqlauth", "dbid", 0);			/* Database ID, given to the SQL service provider */
		userfield	= Conf.ReadValue("sqlauth", "userfield", 0);	/* Field name where username can be found */
		passfield	= Conf.ReadValue("sqlauth", "passfield", 0);	/* Field name where password can be found */
		killreason	= Conf.ReadValue("sqlauth", "killreason", 0);	/* Reason to give when access is denied to a user (put your reg details here) */
		allowpattern= Conf.ReadValue("sqlauth", "allowpattern",0 );	/* Allow nicks matching this pattern without requiring auth */
		encryption	= Conf.ReadValue("sqlauth", "encryption", 0);	/* Name of sql function used to encrypt password, e.g. "md5" or "passwd".
																	 * define, but leave blank if no encryption is to be used.
																	 */
		verbose		= Conf.ReadFlag("sqlauth", "verbose", 0);		/* Set to true if failed connects should be reported to operators */
		
		if (encryption.find("(") == std::string::npos)
		{
			encryption.append("(");
		}
	}	

	virtual void OnUserRegister(userrec* user)
	{
		if ((allowpattern != "") && (Srv->MatchText(user->nick,allowpattern)))
			return;
		
		if (!CheckCredentials(user))
		{
			userrec::QuitUser(Srv,user,killreason);
		}
	}

	bool CheckCredentials(userrec* user)
	{
		Module* target;
		
		target = Srv->FindFeature("SQL");
		
		if(target)
		{
			SQLrequest req = SQLreq(this, target, databaseid, "SELECT ? FROM ? WHERE ? = '?' AND ? = ?'?')", userfield, usertable, userfield, user->nick, passfield, encryption, user->password);
			
			if(req.Send())
			{
				/* When we get the query response from the service provider we will be given an ID to play with,
				 * just an ID number which is unique to this query. We need a way of associating that ID with a userrec
				 * so we insert it into a map mapping the IDs to users.
				 * Thankfully m_sqlutils provides this, it will associate a ID with a user or channel, and if the user quits it removes the
				 * association. This means that if the user quits during a query we will just get a failed lookup from m_sqlutils - telling
				 * us to discard the query.
			 	 */
				ServerInstance->Log(DEBUG, "Sent query, got given ID %lu", req.id);
				
				AssociateUser(this, SQLutils, req.id, user).Send();
					
				return true;
			}
			else
			{
				ServerInstance->Log(DEBUG, "SQLrequest failed: %s", req.error.Str());
			
				if (verbose)
					Srv->WriteOpers("Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick, user->ident, user->host, req.error.Str());
			
				return false;
			}
		}
		else
		{
			ServerInstance->Log(SPARSE, "WARNING: Couldn't find SQL provider module. NOBODY will be allowed to connect until it comes back unless they match an exception");
			return false;
		}
	}
	
	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLRESID, request->GetId()) == 0)
		{
			SQLresult* res;
		
			res = static_cast<SQLresult*>(request);
			
			ServerInstance->Log(DEBUG, "Got SQL result (%s) with ID %lu", res->GetId(), res->id);
			
			userrec* user = GetAssocUser(this, SQLutils, res->id).S().user;
			UnAssociate(this, SQLutils, res->id).S();
			
			if(user)
			{
				if(res->error.Id() == NO_ERROR)
				{				
					ServerInstance->Log(DEBUG, "Associated query ID %lu with user %s", res->id, user->nick);			
					ServerInstance->Log(DEBUG, "Got result with %d rows and %d columns", res->Rows(), res->Cols());
			
					if(res->Rows())
					{
						/* We got a row in the result, this is enough really */
						user->Extend("sqlauthed");
					}
					else if (verbose)
					{
						/* No rows in result, this means there was no record matching the user */
						Srv->WriteOpers("Forbidden connection from %s!%s@%s (SQL query returned no matches)", user->nick, user->ident, user->host);
						user->Extend("sqlauth_failed");
					}
				}
				else if (verbose)
				{
					ServerInstance->Log(DEBUG, "Query failed: %s", res->error.Str());
					Srv->WriteOpers("Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick, user->ident, user->host, res->error.Str());
					user->Extend("sqlauth_failed");
				}
			}
			else
			{
				ServerInstance->Log(DEBUG, "Got query with unknown ID, this probably means the user quit while the query was in progress");
			}
		
			return SQLSUCCESS;
		}
		
		ServerInstance->Log(DEBUG, "Got unsupported API version string: %s", request->GetId());
		
		return NULL;
	}
	
	virtual void OnUserDisconnect(userrec* user)
	{
		user->Shrink("sqlauthed");
		user->Shrink("sqlauth_failed");		
	}
	
	virtual bool OnCheckReady(userrec* user)
	{
		if(user->GetExt("sqlauth_failed"))
		{
			userrec::QuitUser(Srv,user,killreason);
			return false;
		}
		
		return user->GetExt("sqlauthed");
	}

	virtual ~ModuleSQLAuth()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,1,0,VF_VENDOR);
	}
	
};

class ModuleSQLAuthFactory : public ModuleFactory
{
 public:
	ModuleSQLAuthFactory()
	{
	}
	
	~ModuleSQLAuthFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSQLAuth(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSQLAuthFactory;
}
