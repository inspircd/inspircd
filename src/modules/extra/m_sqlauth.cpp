/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "m_sqlv2.h"
#include "m_sqlutils.h"

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table */
/* $ModDep: m_sqlv2.h m_sqlutils.h */

class ModuleSQLAuth : public Module
{
	Module* SQLutils;
	Module* SQLprovider;

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
	: Module::Module(Me)
	{
		ServerInstance->Modules->UseInterface("SQLutils");
		ServerInstance->Modules->UseInterface("SQL");

		SQLutils = ServerInstance->Modules->Find("m_sqlutils.so");
		if (!SQLutils)
			throw ModuleException("Can't find m_sqlutils.so. Please load m_sqlutils.so before m_sqlauth.so.");

		SQLprovider = ServerInstance->Modules->FindFeature("SQL");
		if (!SQLprovider)
			throw ModuleException("Can't find an SQL provider module. Please load one before attempting to load m_sqlauth.");

		OnRehash(NULL,"");
		Implementation eventlist[] = { I_OnUserDisconnect, I_OnCheckReady, I_OnRequest, I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}

	virtual ~ModuleSQLAuth()
	{
		ServerInstance->Modules->DoneWithInterface("SQL");
		ServerInstance->Modules->DoneWithInterface("SQLutils");
	}


	virtual void OnRehash(User* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		
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

	virtual int OnUserRegister(User* user)
	{
		if ((!allowpattern.empty()) && (ServerInstance->MatchText(user->nick,allowpattern)))
		{
			user->Extend("sqlauthed");
			return 0;
		}
		
		if (!CheckCredentials(user))
		{
			User::QuitUser(ServerInstance,user,killreason);
			return 1;
		}
		return 0;
	}

	bool CheckCredentials(User* user)
	{
		SQLrequest req = SQLrequest(this, SQLprovider, databaseid, SQLquery("SELECT ? FROM ? WHERE ? = '?' AND ? = ?'?')") % userfield % usertable % userfield % user->nick %
				passfield % encryption % user->password);
			
		if(req.Send())
		{
			/* When we get the query response from the service provider we will be given an ID to play with,
			 * just an ID number which is unique to this query. We need a way of associating that ID with a User
			 * so we insert it into a map mapping the IDs to users.
			 * Thankfully m_sqlutils provides this, it will associate a ID with a user or channel, and if the user quits it removes the
			 * association. This means that if the user quits during a query we will just get a failed lookup from m_sqlutils - telling
			 * us to discard the query.
		 	 */
			AssociateUser(this, SQLutils, req.id, user).Send();
				
			return true;
		}
		else
		{
			if (verbose)
				ServerInstance->WriteOpers("Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick, user->ident, user->host, req.error.Str());
			return false;
		}
	}
	
	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLRESID, request->GetId()) == 0)
		{
			SQLresult* res = static_cast<SQLresult*>(request);

			User* user = GetAssocUser(this, SQLutils, res->id).S().user;
			UnAssociate(this, SQLutils, res->id).S();
			
			if(user)
			{
				if(res->error.Id() == NO_ERROR)
				{
					if(res->Rows())
					{
						/* We got a row in the result, this is enough really */
						user->Extend("sqlauthed");
					}
					else if (verbose)
					{
						/* No rows in result, this means there was no record matching the user */
						ServerInstance->WriteOpers("Forbidden connection from %s!%s@%s (SQL query returned no matches)", user->nick, user->ident, user->host);
						user->Extend("sqlauth_failed");
					}
				}
				else if (verbose)
				{
					ServerInstance->WriteOpers("Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick, user->ident, user->host, res->error.Str());
					user->Extend("sqlauth_failed");
				}
			}
			else
			{
				return NULL;
			}

			if (!user->GetExt("sqlauthed"))
			{
				User::QuitUser(ServerInstance,user,killreason);
			}
			return SQLSUCCESS;
		}		
		return NULL;
	}
	
	virtual void OnUserDisconnect(User* user)
	{
		user->Shrink("sqlauthed");
		user->Shrink("sqlauth_failed");		
	}
	
	virtual bool OnCheckReady(User* user)
	{
		return user->GetExt("sqlauthed");
	}

	virtual Version GetVersion()
	{
		return Version(1,1,1,0,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleSQLAuth)
