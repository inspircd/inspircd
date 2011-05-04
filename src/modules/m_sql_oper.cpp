/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "command_parse.h"
#include "sql.h"
#include "hash.h"

/* $ModDesc: Allows storage of oper credentials in an SQL table */

static bool OneOfMatches(const char* host, const char* ip, const std::string& hostlist)
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

class OpMeQuery : public SQLQuery
{
 public:
	const std::string uid, username, password;
	OpMeQuery(Module* me, const std::string& u, const std::string& un, const std::string& pw)
		: SQLQuery(me), uid(u), username(un), password(pw)
	{
	}

	void OnResult(SQLResult& res)
	{
		ServerInstance->Logs->Log("m_sqloper",DEBUG, "SQLOPER: result for %s", uid.c_str());
		User* user = ServerInstance->FindNick(uid);
		if (!user || !IS_LOCAL(user))
			return;
		user->frozen = 0;

		// multiple rows may exist
		SQLEntries row;
		while (res.GetRow(row))
		{
			parameterlist cols;
			res.GetCols(cols);

			std::vector<KeyVal>* items;
			reference<ConfigTag> tag = ConfigTag::create("oper", "<m_sqloper>", 0, items);
			for(unsigned int i=0; i < cols.size(); i++)
			{
				if (!row[i].nul)
					items->push_back(KeyVal(cols[i], row[i].value));
			}
			try
			{
				reference<OperInfo> ifo = new OperInfo(tag);

				if (OperUser(user, ifo))
					return;
			}
			catch (CoreException& e)
			{
				ServerInstance->Logs->Log("m_sqloper", DEFAULT, "SQLOPER: Config error in oper %s: %s",
					username.c_str(), e.GetReason());
			}
		}
		ServerInstance->Logs->Log("m_sqloper",DEBUG, "SQLOPER: no matches for %s (checked %d rows)", uid.c_str(), res.Rows());
		// nobody succeeded... fall back to OPER
		fallback(user);
	}

	void OnError(SQLerror& error)
	{
		ServerInstance->Logs->Log("m_sqloper",DEFAULT, "SQLOPER: query failed (%s)", error.Str());
		User* user = ServerInstance->FindNick(uid);
		if (!user || !IS_LOCAL(user))
			return;
		user->frozen = 0;
		fallback(user);
	}

	void fallback(User* user)
	{
		Command* oper_command = ServerInstance->Parser->GetHandler("OPER");

		if (oper_command)
		{
			std::vector<std::string> params;
			params.push_back(username);
			params.push_back(password);
			oper_command->Handle(params, user);
		}
		else
		{
			ServerInstance->Logs->Log("m_sqloper",SPARSE, "BUG: WHAT?! Why do we have no OPER command?!");
		}
	}

	bool OperUser(User* user, OperInfo* ifo)
	{
		std::string pattern = ifo->getConfig("host");
		std::string hostname(user->ident);

		hostname.append("@").append(user->host);

		if (OneOfMatches(hostname.c_str(), user->GetIPString(), pattern.c_str()))
		{
			/* Opertype and host match, looks like this is it. */

			user->Oper(ifo);
			return true;
		}

		return false;
	}
};

class ModuleSQLOper : public Module
{
	std::string query;
	std::string hashtype;
	dynamic_reference<SQLProvider> SQL;

public:
	ModuleSQLOper() : SQL("SQL") {}

	void init()
	{

		Implementation eventlist[] = { I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* tag = ServerInstance->Config->GetTag("sqloper");

		std::string dbid = tag->getString("dbid");
		if (dbid.empty())
			SQL.SetProvider("SQL");
		else
			SQL.SetProvider("SQL/" + dbid);
		hashtype = tag->getString("hash");
		query = tag->getString("query", "SELECT hostname as host, type FROM ircd_opers WHERE username='$username' AND password='$password'");
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		if (validated && command == "OPER" && parameters.size() >= 2)
		{
			if (SQL)
			{
				// freeze the user's recvq while we wait for SQL
				// this ensures that the result of the /OPER is returned in order
				user->frozen = 1;
				LookupOper(user, parameters[0], parameters[1]);
				/* Query is in progress, it will re-invoke OPER if needed */
				return MOD_RES_DENY;
			}
			ServerInstance->Logs->Log("m_sqloper",DEFAULT, "SQLOPER: database not present");
		}
		return MOD_RES_PASSTHRU;
	}

	void LookupOper(User* user, const std::string &username, const std::string &password)
	{
		HashProvider* hash = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + hashtype);

		ParamM userinfo;
		user->PopulateInfoMap(userinfo);
		userinfo["username"] = username;
		userinfo["password"] = hash ? hash->hexsum(password) : password;

		SQL->submit(new OpMeQuery(this, user->uuid, username, password), query, userinfo);
	}

	Version GetVersion()
	{
		return Version("Allows storage of oper credentials in an SQL table", VF_VENDOR);
	}

};

MODULE_INIT(ModuleSQLOper)
