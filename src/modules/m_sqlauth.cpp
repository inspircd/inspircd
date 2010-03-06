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
#include "sql.h"
#include "hash.h"

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table */

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
				ServerInstance->SNO->WriteGlobalSno('a', "Forbidden connection from %s!%s@%s (SQL query returned no matches)", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
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
			ServerInstance->SNO->WriteGlobalSno('a', "Forbidden connection from %s!%s@%s (SQL query failed: %s)", user->nick.c_str(), user->ident.c_str(), user->host.c_str(), error.Str());
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
	ModuleSQLAuth() : pendingExt("sqlauth-wait", this), SQL("SQL")
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(pendingExt);
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnUserDisconnect, I_OnCheckReady, I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 4);
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
		SQL.lookup();
	}

	void OnUserRegister(LocalUser* user)
	{
		// Note this is their initial (unresolved) connect block
		ConfigTag* tag = user->MyClass->config;
		if (!tag->getBool("usesqlauth", true))
			return;

		if (!allowpattern.empty() && InspIRCd::Match(user->nick,allowpattern))
			return;

		if (pendingExt.get(user))
			return;

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
		return Version("Allow/Deny connections based upon an arbitary SQL table", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSQLAuth)
