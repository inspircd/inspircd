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
#include <map>

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "helperfuncs.h"
#include "m_sqlv2.h"

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table */

typedef std::map<unsigned int, userrec*> QueryUserMap;

class ModuleSQLAuth : public Module
{
	Server* Srv;

	std::string usertable;
	std::string userfield;
	std::string passfield;
	std::string encryption;
	std::string killreason;
	std::string allowpattern;
	std::string databaseid;
	
	bool verbose;
	
	QueryUserMap qumap;
	
public:
	ModuleSQLAuth(Server* Me)
	: Module::Module(Me)
	{
		Srv = Me;
		OnRehash("");
	}

	void Implements(char* List)
	{
		List[I_OnUserDisconnect] = List[I_OnCheckReady] = List[I_OnRequest] = List[I_OnRehash] = List[I_OnUserRegister] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		ConfigReader Conf;
		
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
			Srv->QuitUser(user,killreason);
		}
	}

	bool CheckCredentials(userrec* user)
	{
		bool found;
		Module* target;
		
		found = false;
		target = Srv->FindFeature("SQL");
		
		if(target)
		{
			SQLrequest req = SQLreq(this, target, databaseid, "SELECT ? FROM ? WHERE ? = '?' AND ? = ?'?')", userfield, usertable, userfield, user->nick, passfield, encryption, user->password);
			
			if(req.Send())
			{
				/* When we get the query response from the service provider we will be given an ID to play with,
				 * just an ID number which is unique to this query. We need a way of associating that ID with a userrec
				 * so we insert it into a map mapping the IDs to users.
				 * This isn't quite enough though, as if the user quit while the query was in progress then when the result
				 * came to be processed we'd get an invalid userrec* out of the map. Now we *could* solve this by watching
				 * OnUserDisconnect() and iterating the map every time someone quits to make sure they didn't have any queries
				 * in progress, but that would be relatively slow and inefficient. Instead (thanks to w00t ;p) we attach a list
				 * of query IDs associated with it to the userrec, so in OnUserDisconnect() we can remove it immediately.
				 */
				log(DEBUG, "Sent query, got given ID %lu", req.id);
				qumap.insert(std::make_pair(req.id, user));
				
				if(!user->Extend("sqlauth_queryid", new unsigned long(req.id)))
				{
					log(DEBUG, "BUG: user being sqlauth'd already extended with 'sqlauth_queryid' :/");
				}
				
				return true;
			}
			else
			{
				log(DEBUG, "SQLrequest failed: %s", req.error.Str());
				
				if (verbose)
					WriteOpers("Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick, user->ident, user->host, req.error.Str());
				
				return false;
			}
		}
		else
		{
			log(SPARSE, "WARNING: Couldn't find SQL provider module. NOBODY will be allowed to connect until it comes back unless they match an exception");
			return false;
		}
	}
	
	virtual char* OnRequest(Request* request)
	{
		if(strcmp(SQLRESID, request->GetData()) == 0)
		{
			SQLresult* res;
			QueryUserMap::iterator iter;
		
			res = static_cast<SQLresult*>(request);
			
			log(DEBUG, "Got SQL result (%s) with ID %lu", res->GetData(), res->id);
			
			iter = qumap.find(res->id);
			
			if(iter != qumap.end())
			{
				userrec* user;
				unsigned long* id;
				
				user = iter->second;
				
				/* Remove our ID from the lookup table to keep it as small and neat as possible */
				qumap.erase(iter);
				
				/* Cleanup the userrec, no point leaving this here */
				if(user->GetExt("sqlauth_queryid", id))
				{
					user->Shrink("sqlauth_queryid");
					delete id;
				}
				
				if(res->error.Id() == NO_ERROR)
				{				
					log(DEBUG, "Associated query ID %lu with user %s", res->id, user->nick);			
					log(DEBUG, "Got result with %d rows and %d columns", res->Rows(), res->Cols());
			
					if(res->Rows())
					{
						/* We got a row in the result, this is enough really */
						user->Extend("sqlauthed");
					}
					else if (verbose)
					{
						/* No rows in result, this means there was no record matching the user */
						WriteOpers("Forbidden connection from %s!%s@%s (SQL query returned no matches)", user->nick, user->ident, user->host);
						user->Extend("sqlauth_failed");
					}
				}
				else if (verbose)
				{
					log(DEBUG, "Query failed: %s", res->error.Str());
					WriteOpers("Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick, user->ident, user->host, res->error.Str());
					user->Extend("sqlauth_failed");
				}
			}
			else
			{
				log(DEBUG, "Got query with unknown ID, this probably means the user quit while the query was in progress");
			}
		
			return SQLSUCCESS;
		}
		
		log(DEBUG, "Got unsupported API version string: %s", request->GetData());
		
		return NULL;
	}
	
	virtual void OnUserDisconnect(userrec* user)
	{
		unsigned long* id;
		
		if(user->GetExt("sqlauth_queryid", id))
		{
			QueryUserMap::iterator iter;
			
			iter = qumap.find(*id);
			
			if(iter != qumap.end())
			{
				if(iter->second == user)
				{
					qumap.erase(iter);
					
					log(DEBUG, "Erased query from map associated with quitting user %s", user->nick);
				}
				else
				{
					log(DEBUG, "BUG: ID associated with user %s doesn't have the same userrec* associated with it in the map");
				}		
			}
			else
			{
				log(DEBUG, "BUG: user %s was extended with sqlauth_queryid but there was nothing matching in the map", user->nick);
			}
			
			user->Shrink("sqlauth_queryid");
			delete id;
		}

		user->Shrink("sqlauthed");
		user->Shrink("sqlauth_failed");		
	}
	
	virtual bool OnCheckReady(userrec* user)
	{
		if(user->GetExt("sqlauth_failed"))
		{
			Srv->QuitUser(user,killreason);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSQLAuth(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSQLAuthFactory;
}
