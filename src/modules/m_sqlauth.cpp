/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
#include "hash.h"

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table */

class ModuleSQLAuth : public Module
{
	LocalIntExt sqlAuthed;
	Module* SQLutils;
	Module* SQLprovider;

	std::string freeformquery;
	std::string killreason;
	std::string allowpattern;
	std::string databaseid;

	bool verbose;

public:
	ModuleSQLAuth() : sqlAuthed("sqlauth", this)
	{
		SQLutils = ServerInstance->Modules->Find("m_sqlutils.so");
		if (!SQLutils)
			throw ModuleException("Can't find m_sqlutils.so. Please load m_sqlutils.so before m_sqlauth.so.");

		ServiceProvider* prov = ServerInstance->Modules->FindService(SERVICE_DATA, "SQL");
		if (!prov)
			throw ModuleException("Can't find an SQL provider module. Please load one before attempting to load m_sqlauth.");
		SQLprovider = prov->creator;

		OnRehash(NULL);
		Implementation eventlist[] = { I_OnUserDisconnect, I_OnCheckReady, I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	virtual ~ModuleSQLAuth()
	{
	}

	void OnRehash(User* user)
	{
		ConfigReader Conf;

		databaseid	= Conf.ReadValue("sqlauth", "dbid", 0);			/* Database ID, given to the SQL service provider */
		freeformquery	= Conf.ReadValue("sqlauth", "query", 0);	/* Field name where username can be found */
		killreason	= Conf.ReadValue("sqlauth", "killreason", 0);	/* Reason to give when access is denied to a user (put your reg details here) */
		allowpattern	= Conf.ReadValue("sqlauth", "allowpattern",0 );	/* Allow nicks matching this pattern without requiring auth */
		verbose		= Conf.ReadFlag("sqlauth", "verbose", 0);		/* Set to true if failed connects should be reported to operators */
	}

	ModResult OnUserRegister(LocalUser* user)
	{
		if ((!allowpattern.empty()) && (InspIRCd::Match(user->nick,allowpattern)))
		{
			sqlAuthed.set(user, 1);
			return MOD_RES_PASSTHRU;
		}

		if (!CheckCredentials(user))
		{
			ServerInstance->Users->QuitUser(user, killreason);
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	bool CheckCredentials(LocalUser* user)
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

		HashProvider* md5 = ServerInstance->Modules->FindDataService<HashProvider>("hash/md5");
		if (md5)
			SearchAndReplace(thisquery, std::string("$md5pass"), md5->hexsum(user->password));

		HashProvider* sha256 = ServerInstance->Modules->FindDataService<HashProvider>("hash/sha256");
		if (sha256)
			SearchAndReplace(thisquery, std::string("$sha256pass"), sha256->hexsum(user->password));

		/* Build the query */
		SQLrequest req = SQLrequest(this, SQLprovider, databaseid, SQLquery(thisquery));

		req.Send();
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

	void OnRequest(Request& request)
	{
		if(strcmp(SQLRESID, request.id) == 0)
		{
			SQLresult* res = static_cast<SQLresult*>(&request);

			User* user = GetAssocUser(this, SQLutils, res->id).S().user;
			UnAssociate(this, SQLutils, res->id).S();

			if(user)
			{
				if(res->error.Id() == SQL_NO_ERROR)
				{
					if(res->Rows())
					{
						/* We got a row in the result, this is enough really */
						sqlAuthed.set(user, 1);
					}
					else if (verbose)
					{
						/* No rows in result, this means there was no record matching the user */
						ServerInstance->SNO->WriteGlobalSno('a', "Forbidden connection from %s!%s@%s (SQL query returned no matches)", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
					}
				}
				else if (verbose)
				{
					ServerInstance->SNO->WriteGlobalSno('a', "Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), res->error.Str());
				}
			}
			else
			{
				return;
			}

			if (!sqlAuthed.get(user))
			{
				ServerInstance->Users->QuitUser(user, killreason);
			}
		}
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		return sqlAuthed.get(user) ? MOD_RES_PASSTHRU : MOD_RES_DENY;
	}

	Version GetVersion()
	{
		return Version("Allow/Deny connections based upon an arbitary SQL table", VF_VENDOR);
	}

};

MODULE_INIT(ModuleSQLAuth)
