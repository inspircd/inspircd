/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "modules/sql.h"
#include "modules/hash.h"

class OpMeQuery : public SQLQuery
{
 public:
	const std::string uid, username, password;
	OpMeQuery(Module* me, const std::string& u, const std::string& un, const std::string& pw)
		: SQLQuery(me), uid(u), username(un), password(pw)
	{
	}

	void OnResult(SQLResult& res) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "result for %s", uid.c_str());
		User* user = ServerInstance->FindNick(uid);
		if (!user)
			return;

		// multiple rows may exist
		SQLEntries row;
		while (res.GetRow(row))
		{
			if (OperUser(user, row[0], row[1]))
				return;
		}
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "no matches for %s (checked %d rows)", uid.c_str(), res.Rows());
		// nobody succeeded... fall back to OPER
		fallback();
	}

	void OnError(SQLerror& error) CXX11_OVERRIDE
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "query failed (%s)", error.Str());
		fallback();
	}

	void fallback()
	{
		User* user = ServerInstance->FindNick(uid);
		if (!user)
			return;

		Command* oper_command = ServerInstance->Parser.GetHandler("OPER");

		if (oper_command)
		{
			std::vector<std::string> params;
			params.push_back(username);
			params.push_back(password);
			oper_command->Handle(params, user);
		}
		else
		{
			ServerInstance->Logs->Log(MODNAME, LOG_SPARSE, "BUG: WHAT?! Why do we have no OPER command?!");
		}
	}

	bool OperUser(User* user, const std::string &pattern, const std::string &type)
	{
		ServerConfig::OperIndex::const_iterator iter = ServerInstance->Config->OperTypes.find(type);
		if (iter == ServerInstance->Config->OperTypes.end())
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "bad type '%s' in returned row for oper %s", type.c_str(), username.c_str());
			return false;
		}
		OperInfo* ifo = iter->second;

		std::string hostname(user->ident);

		hostname.append("@").append(user->host);

		if (InspIRCd::MatchMask(pattern, hostname, user->GetIPString()))
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
	ModuleSQLOper() : SQL(this, "SQL") {}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("sqloper");

		std::string dbid = tag->getString("dbid");
		if (dbid.empty())
			SQL.SetProvider("SQL");
		else
			SQL.SetProvider("SQL/" + dbid);

		hashtype = tag->getString("hash");
		query = tag->getString("query", "SELECT hostname as host, type FROM ircd_opers WHERE username='$username' AND password='$password' AND active=1;");
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string &original_line) CXX11_OVERRIDE
	{
		if (validated && command == "OPER" && parameters.size() >= 2)
		{
			if (SQL)
			{
				LookupOper(user, parameters[0], parameters[1]);
				/* Query is in progress, it will re-invoke OPER if needed */
				return MOD_RES_DENY;
			}
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "database not present");
		}
		return MOD_RES_PASSTHRU;
	}

	void LookupOper(User* user, const std::string &username, const std::string &password)
	{
		HashProvider* hash = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + hashtype);

		ParamM userinfo;
		SQL->PopulateUserInfo(user, userinfo);
		userinfo["username"] = username;
		userinfo["password"] = hash ? hash->Generate(password) : password;

		SQL->submit(new OpMeQuery(this, user->uuid, username, password), query, userinfo);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows storage of oper credentials in an SQL table", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSQLOper)
