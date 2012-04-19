/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "m_sqlv2.h"
#include "m_sqlutils.h"
#include "m_hash.h"

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table */
/* $ModDep: m_sqlv2.h m_sqlutils.h m_hash.h */

class ModuleSQLAuth : public Module
{
	Module* SQLutils;
	Module* SQLprovider;

	std::string freeformquery;
	std::string killreason;
	std::string allowpattern;
	std::string databaseid;

	bool verbose;

public:
	ModuleSQLAuth(InspIRCd* Me)
	: Module(Me)
	{
		ServerInstance->Modules->UseInterface("SQLutils");
		ServerInstance->Modules->UseInterface("SQL");

		SQLutils = ServerInstance->Modules->Find("m_sqlutils.so");
		if (!SQLutils)
			throw ModuleException("Can't find m_sqlutils.so. Please load m_sqlutils.so before m_sqlauth.so.");

		SQLprovider = ServerInstance->Modules->FindFeature("SQL");
		if (!SQLprovider)
			throw ModuleException("Can't find an SQL provider module. Please load one before attempting to load m_sqlauth.");

		OnRehash(NULL);
		Implementation eventlist[] = { I_OnUserDisconnect, I_OnCheckReady, I_OnRequest, I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 5);
	}

	virtual ~ModuleSQLAuth()
	{
		ServerInstance->Modules->DoneWithInterface("SQL");
		ServerInstance->Modules->DoneWithInterface("SQLutils");
	}


	virtual void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);

		databaseid	= Conf.ReadValue("sqlauth", "dbid", 0);			/* Database ID, given to the SQL service provider */
		freeformquery	= Conf.ReadValue("sqlauth", "query", 0);	/* Field name where username can be found */
		killreason	= Conf.ReadValue("sqlauth", "killreason", 0);	/* Reason to give when access is denied to a user (put your reg details here) */
		allowpattern	= Conf.ReadValue("sqlauth", "allowpattern",0 );	/* Allow nicks matching this pattern without requiring auth */
		verbose		= Conf.ReadFlag("sqlauth", "verbose", 0);		/* Set to true if failed connects should be reported to operators */
	}

	virtual int OnUserRegister(User* user)
	{
		if ((!allowpattern.empty()) && (InspIRCd::Match(user->nick,allowpattern)))
		{
			user->Extend("sqlauthed");
			return 0;
		}

		if (!CheckCredentials(user))
		{
			ServerInstance->Users->QuitUser(user, killreason);
			return 1;
		}
		return 0;
	}

	bool CheckCredentials(User* user)
	{
		std::string thisquery = freeformquery;
		std::string safepass = user->password;
		std::string safegecos = user->fullname;

		/* Search and replace the escaped nick and escaped pass into the query */

		SearchAndReplace(safepass, std::string("\""), std::string("\\\""));
		SearchAndReplace(safegecos, std::string("\""), std::string("\\\""));

		SearchAndReplace(thisquery, std::string("$nick"), user->nick);
		SearchAndReplace(thisquery, std::string("$pass"), safepass);
		SearchAndReplace(thisquery, std::string("$host"), user->host);
		SearchAndReplace(thisquery, std::string("$ip"), std::string(user->GetIPString()));
		SearchAndReplace(thisquery, std::string("$gecos"), safegecos);
		SearchAndReplace(thisquery, std::string("$ident"), user->ident);
		SearchAndReplace(thisquery, std::string("$server"), std::string(user->server));
		SearchAndReplace(thisquery, std::string("$uuid"), user->uuid);

		Module* HashMod = ServerInstance->Modules->Find("m_md5.so");

		if (HashMod)
		{
			HashResetRequest(this, HashMod).Send();
			SearchAndReplace(thisquery, std::string("$md5pass"), std::string(HashSumRequest(this, HashMod, user->password).Send()));
		}

		HashMod = ServerInstance->Modules->Find("m_sha256.so");

		if (HashMod)
		{
			HashResetRequest(this, HashMod).Send();
			SearchAndReplace(thisquery, std::string("$sha256pass"), std::string(HashSumRequest(this, HashMod, user->password).Send()));
		}

		/* Build the query */
		SQLrequest req = SQLrequest(this, SQLprovider, databaseid, SQLquery(thisquery));

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
				ServerInstance->SNO->WriteGlobalSno('a', "Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), req.error.Str());
			return false;
		}
	}

	virtual const char* OnRequest(Request* request)
	{
		if(strcmp(SQLRESID, request->GetId()) == 0)
		{
			SQLresult* res = static_cast<SQLresult*>(request);

			User* user = GetAssocUser(this, SQLutils, res->id).S().user;
			UnAssociate(this, SQLutils, res->id).S();

			if(user)
			{
				if(res->error.Id() == SQL_NO_ERROR)
				{
					if(res->Rows())
					{
						/* We got a row in the result, this is enough really */
						user->Extend("sqlauthed");
					}
					else if (verbose)
					{
						/* No rows in result, this means there was no record matching the user */
						ServerInstance->SNO->WriteGlobalSno('a', "Forbidden connection from %s!%s@%s (SQL query returned no matches)", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
						user->Extend("sqlauth_failed");
					}
				}
				else if (verbose)
				{
					ServerInstance->SNO->WriteGlobalSno('a', "Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), res->error.Str());
					user->Extend("sqlauth_failed");
				}
			}
			else
			{
				return NULL;
			}

			if (!user->GetExt("sqlauthed"))
			{
				ServerInstance->Users->QuitUser(user, killreason);
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
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSQLAuth)
