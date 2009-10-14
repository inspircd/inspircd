/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "m_sqlv2.h"
#include "m_sqlutils.h"
#include "m_hash.h"

/* $ModDesc: Allows storage of oper credentials in an SQL table */
/* $ModDep: m_sqlv2.h m_sqlutils.h m_hash.h */

typedef std::map<irc::string, Module*> hashymodules;

class ModuleSQLOper : public Module
{
	LocalStringExt saved_user;
	LocalStringExt saved_pass;
	Module* SQLutils;
	std::string databaseid;
	irc::string hashtype;
	hashymodules hashers;
	bool diduseiface;
	parameterlist names;

public:
	ModuleSQLOper() : saved_user("sqloper_user", this), saved_pass("sqloper_pass", this)
	{
		ServerInstance->Modules->UseInterface("SQLutils");
		ServerInstance->Modules->UseInterface("SQL");
		ServerInstance->Modules->UseInterface("HashRequest");

		OnRehash(NULL);

		diduseiface = false;

		/* Find all modules which implement the interface 'HashRequest' */
		modulelist* ml = ServerInstance->Modules->FindInterface("HashRequest");

		/* Did we find any modules? */
		if (ml)
		{
			/* Yes, enumerate them all to find out the hashing algorithm name */
			for (modulelist::iterator m = ml->begin(); m != ml->end(); m++)
			{
				/* Make a request to it for its name, its implementing
				 * HashRequest so we know its safe to do this
				 */
				std::string name = HashNameRequest(this, *m).response;
				/* Build a map of them */
				hashers[name.c_str()] = *m;
				names.push_back(name);
			}
			/* UseInterface doesn't do anything if there are no providers, so we'll have to call it later if a module gets loaded later on. */
			diduseiface = true;
			ServerInstance->Modules->UseInterface("HashRequest");
		}

		SQLutils = ServerInstance->Modules->Find("m_sqlutils.so");
		if (!SQLutils)
			throw ModuleException("Can't find m_sqlutils.so. Please load m_sqlutils.so before m_sqloper.so.");

		Implementation eventlist[] = { I_OnRehash, I_OnPreCommand, I_OnLoadModule };
		ServerInstance->Modules->Attach(eventlist, this, 4);
	}

	bool OneOfMatches(const char* host, const char* ip, const char* hostlist)
	{
		std::stringstream hl(hostlist);
		std::string xhost;
		while (hl >> xhost)
		{
			if (InspIRCd::Match(host, xhost, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(ip, xhost, ascii_case_insensitive_map))
			{
				return true;
			}
		}
		return false;
	}

	virtual void OnLoadModule(Module* mod)
	{
		if (ServerInstance->Modules->ModuleHasInterface(mod, "HashRequest"))
		{
			std::string sname = HashNameRequest(this, mod).response;
			hashers[sname.c_str()] = mod;
			names.push_back(sname);
			if (!diduseiface)
			{
				ServerInstance->Modules->UseInterface("HashRequest");
				diduseiface = true;
			}
		}
	}

	virtual ~ModuleSQLOper()
	{
		ServerInstance->Modules->DoneWithInterface("SQL");
		ServerInstance->Modules->DoneWithInterface("SQLutils");
		if (diduseiface)
			ServerInstance->Modules->DoneWithInterface("HashRequest");
	}


	virtual void OnRehash(User* user)
	{
		ConfigReader Conf;

		databaseid = Conf.ReadValue("sqloper", "dbid", 0); /* Database ID of a database configured for the service provider module */
		hashtype = assign(Conf.ReadValue("sqloper", "hash", 0));
	}

	virtual ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, User *user, bool validated, const std::string &original_line)
	{
		if ((validated) && (command == "OPER"))
		{
			if (LookupOper(user, parameters[0], parameters[1]))
			{
				/* Returning true here just means the query is in progress, or on it's way to being
				 * in progress. Nothing about the /oper actually being successful..
				 * If the oper lookup fails later, we pass the command to the original handler
				 * for /oper by calling its Handle method directly.
				 */
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}

	bool LookupOper(User* user, const std::string &username, const std::string &password)
	{
		Module* target;

		target = ServerInstance->Modules->FindFeature("SQL");

		if (target)
		{
			hashymodules::iterator x = hashers.find(hashtype);
			if (x == hashers.end())
				return false;

			/* Make an MD5 hash of the password for using in the query */
			std::string md5_pass_hash = HashRequest(this, x->second, password).hex();

			/* We generate our own sum here because some database providers (e.g. SQLite) dont have a builtin md5/sha256 function,
			 * also hashing it in the module and only passing a remote query containing a hash is more secure.
			 */
			SQLrequest req = SQLrequest(this, target, databaseid,
					SQLquery("SELECT username, password, hostname, type FROM ircd_opers WHERE username = '?' AND password='?'") % username % md5_pass_hash);

			/* When we get the query response from the service provider we will be given an ID to play with,
			 * just an ID number which is unique to this query. We need a way of associating that ID with a User
			 * so we insert it into a map mapping the IDs to users.
			 * Thankfully m_sqlutils provides this, it will associate a ID with a user or channel, and if the user quits it removes the
			 * association. This means that if the user quits during a query we will just get a failed lookup from m_sqlutils - telling
			 * us to discard the query.
			 */
			AssociateUser(this, SQLutils, req.id, user).Send();

			saved_user.set(user, username);
			saved_pass.set(user, password);

			return true;
		}
		else
		{
			ServerInstance->Logs->Log("m_sqloper",SPARSE, "WARNING: Couldn't find SQL provider module. NOBODY will be able to oper up unless their o:line is statically configured");
			return false;
		}
	}

	void OnRequest(Request& request)
	{
		if (strcmp(SQLRESID, request.id) == 0)
		{
			SQLresult* res = static_cast<SQLresult*>(&request);

			User* user = GetAssocUser(this, SQLutils, res->id).S().user;
			UnAssociate(this, SQLutils, res->id).S();

			if (user)
			{
				std::string* tried_user = saved_user.get(user);
				std::string* tried_pass = saved_pass.get(user);
				if (res->error.Id() == SQL_NO_ERROR)
				{
					if (res->Rows())
					{
						/* We got a row in the result, this means there was a record for the oper..
						 * now we just need to check if their host matches, and if it does then
						 * oper them up.
						 *
						 * We now (previous versions of the module didn't) support multiple SQL
						 * rows per-oper in the same way the config file does, all rows will be tried
						 * until one is found which matches. This is useful to define several different
						 * hosts for a single oper.
						 *
						 * The for() loop works as SQLresult::GetRowMap() returns an empty map when there
						 * are no more rows to return.
						 */

						for (SQLfieldMap& row = res->GetRowMap(); row.size(); row = res->GetRowMap())
						{
							if (OperUser(user, row["hostname"].d, row["type"].d))
							{
								/* If/when one of the rows matches, stop checking and return */
								saved_user.unset(user);
								saved_pass.unset(user);
							}
							if (tried_user && tried_pass)
							{
								LoginFail(user, *tried_user, *tried_pass);
								saved_user.unset(user);
								saved_pass.unset(user);
							}
						}
					}
					else
					{
						/* No rows in result, this means there was no oper line for the user,
						 * we should have already checked the o:lines so now we need an
						 * "insufficient awesomeness" (invalid credentials) error
						 */
						if (tried_user && tried_pass)
						{
							LoginFail(user, *tried_user, *tried_pass);
							saved_user.unset(user);
							saved_pass.unset(user);
						}
					}
				}
				else
				{
					/* This one shouldn't happen, the query failed for some reason.
					 * We have to fail the /oper request and give them the same error
					 * as above.
					 */
					if (tried_user && tried_pass)
					{
						LoginFail(user, *tried_user, *tried_pass);
						saved_user.unset(user);
						saved_pass.unset(user);
					}

				}
			}
		}
	}

	void LoginFail(User* user, const std::string &username, const std::string &pass)
	{
		Command* oper_command = ServerInstance->Parser->GetHandler("OPER");

		if (oper_command)
		{
			std::vector<std::string> params;
			params.push_back(username);
			params.push_back(pass);
			oper_command->Handle(params, user);
		}
		else
		{
			ServerInstance->Logs->Log("m_sqloper",DEBUG, "BUG: WHAT?! Why do we have no OPER command?!");
		}
	}

	bool OperUser(User* user, const std::string &pattern, const std::string &type)
	{
		ConfigReader Conf;

		for (int j = 0; j < Conf.Enumerate("type"); j++)
		{
			std::string tname = Conf.ReadValue("type","name",j);
			std::string hostname(user->ident);

			hostname.append("@").append(user->host);

			if ((tname == type) && OneOfMatches(hostname.c_str(), user->GetIPString(), pattern.c_str()))
			{
				/* Opertype and host match, looks like this is it. */
				std::string operhost = Conf.ReadValue("type", "host", j);

				if (operhost.size())
					user->ChangeDisplayedHost(operhost.c_str());

				user->Oper(type, tname);
				return true;
			}
		}

		return false;
	}

	Version GetVersion()
	{
		return Version("Allows storage of oper credentials in an SQL table", VF_VENDOR);
	}

};

MODULE_INIT(ModuleSQLOper)
