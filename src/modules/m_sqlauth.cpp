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
#include "sql.h"
#include "hash.h"

/* $ModDesc: Allow/Deny connections based upon an arbitrary SQL table */

enum AuthState {
	AUTH_STATE_NONE = 0,
	AUTH_STATE_BUSY = 1,
	AUTH_STATE_FAIL = 2
};

class AuthQuery : public SQLQuery
{
 public:
	const std::string uid;
	LocalIntExt& pendingExt;
	bool verbose;
	AuthQuery(Module* me, const std::string& u, LocalIntExt& e, bool v)
		: SQLQuery(me), uid(u), pendingExt(e), verbose(v)
	{
	}
	
	void OnResult(SQLResult& res)
	{
		User* user = ServerInstance->FindNick(uid);
		if (!user)
			return;
		if (res.Rows())
		{
			pendingExt.set(user, AUTH_STATE_NONE);
		}
		else
		{
			if (verbose)
				ServerInstance->SNO->WriteGlobalSno('a', "Forbidden connection from %s (SQL query returned no matches)", user->GetFullRealHost().c_str());
			pendingExt.set(user, AUTH_STATE_FAIL);
		}
	}

	void OnError(SQLerror& error)
	{
		User* user = ServerInstance->FindNick(uid);
		if (!user)
			return;
		pendingExt.set(user, AUTH_STATE_FAIL);
		if (verbose)
			ServerInstance->SNO->WriteGlobalSno('a', "Forbidden connection from %s (SQL query failed: %s)", user->GetFullRealHost().c_str(), error.Str());
	}
};

class ModuleSQLAuth : public Module
{
	LocalIntExt pendingExt;
	dynamic_reference<SQLProvider> SQL;

	std::string freeformquery;
	std::string killreason;
	std::string allowpattern;
	bool verbose;

 public:
	ModuleSQLAuth() : pendingExt("sqlauth-wait", this), SQL(this, "SQL")
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(pendingExt);
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnCheckReady, I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User* user)
	{
		ConfigTag* conf = ServerInstance->Config->ConfValue("sqlauth");
		std::string dbid = conf->getString("dbid");
		if (dbid.empty())
			SQL.SetProvider("SQL");
		else
			SQL.SetProvider("SQL/" + dbid);
		freeformquery = conf->getString("query");
		killreason = conf->getString("killreason");
		allowpattern = conf->getString("allowpattern");
		verbose = conf->getBool("verbose");
	}

	ModResult OnUserRegister(LocalUser* user)
	{
		// Note this is their initial (unresolved) connect block
		ConfigTag* tag = user->MyClass->config;
		if (!tag->getBool("usesqlauth", true))
			return MOD_RES_PASSTHRU;

		if (!allowpattern.empty() && InspIRCd::Match(user->nick,allowpattern))
			return MOD_RES_PASSTHRU;

		if (pendingExt.get(user))
			return MOD_RES_PASSTHRU;

		if (!SQL)
		{
			ServerInstance->SNO->WriteGlobalSno('a', "Forbiding connection from %s (SQL database not present)", user->GetFullRealHost().c_str());
			ServerInstance->Users->QuitUser(user, killreason);
			return MOD_RES_PASSTHRU;
		}

		pendingExt.set(user, AUTH_STATE_BUSY);

		ParamM userinfo;
		SQL->PopulateUserInfo(user, userinfo);
		userinfo["pass"] = user->password;

		HashProvider* md5 = ServerInstance->Modules->FindDataService<HashProvider>("hash/md5");
		if (md5)
			userinfo["md5pass"] = md5->hexsum(user->password);

		HashProvider* sha256 = ServerInstance->Modules->FindDataService<HashProvider>("hash/sha256");
		if (sha256)
			userinfo["sha256pass"] = sha256->hexsum(user->password);

		SQL->submit(new AuthQuery(this, user->uuid, pendingExt, verbose), freeformquery, userinfo);

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		switch (pendingExt.get(user))
		{
			case AUTH_STATE_NONE:
				return MOD_RES_PASSTHRU;
			case AUTH_STATE_BUSY:
				return MOD_RES_DENY;
			case AUTH_STATE_FAIL:
				ServerInstance->Users->QuitUser(user, killreason);
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Allow/Deny connections based upon an arbitrary SQL table", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSQLAuth)
