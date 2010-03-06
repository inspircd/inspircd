/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
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
	OpMeQuery(Module* me, const std::string& db, const std::string& q, const std::string& u, const std::string& un, const std::string& pw)
		: SQLQuery(me, db, q), uid(u), username(un), password(pw) {}

	void OnResult(SQLResult& res)
	{
		User* user = ServerInstance->FindNick(uid);
		if (!user)
			return;

		// multiple rows may exist for multiple hosts
		parameterlist row;
		while (res.GetRow(row))
		{
			if (OperUser(user, row[2], row[3]))
				return;
		}
		// nobody succeeded... fall back to OPER
		fallback();
	}

	void OnError(SQLerror& error)
	{
		fallback();
	}

	void fallback()
	{
		User* user = ServerInstance->FindNick(uid);
		if (!user)
			return;

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

	bool OperUser(User* user, const std::string &pattern, const std::string &type)
	{
		OperIndex::iterator iter = ServerInstance->Config->oper_blocks.find(" " + type);
		if (iter == ServerInstance->Config->oper_blocks.end())
			return false;
		OperInfo* ifo = iter->second;

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
	std::string databaseid;
	std::string hashtype;
	dynamic_reference<SQLProvider> SQL;

public:
	ModuleSQLOper() : SQL(this, "SQL") {}

	void init()
	{
		OnRehash(NULL);

		Implementation eventlist[] = { I_OnRehash, I_OnPreCommand };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	void OnRehash(User* user)
	{
		ConfigReader Conf;

		databaseid = Conf.ReadValue("sqloper", "dbid", 0); /* Database ID of a database configured for the service provider module */
		hashtype = Conf.ReadValue("sqloper", "hash", 0);
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line)
	{
		if (validated && command == "OPER" && parameters.size() == 2 && SQL)
		{
			LookupOper(user, parameters[0], parameters[1]);
			/* Query is in progress, it will re-invoke OPER if needed */
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void LookupOper(User* user, const std::string &username, const std::string &password)
	{
		HashProvider* hash = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + hashtype);

		parameterlist params;
		params.push_back(username);
		params.push_back(hash ? hash->hexsum(password) : password);

		SQL->submit(new OpMeQuery(this, databaseid, SQL->FormatQuery(
			"SELECT username, password, hostname, type FROM ircd_opers WHERE username = '?' AND password='?'", params
			), user->uuid, username, password));
	}

	Version GetVersion()
	{
		return Version("Allows storage of oper credentials in an SQL table", VF_VENDOR);
	}

};

MODULE_INIT(ModuleSQLOper)
