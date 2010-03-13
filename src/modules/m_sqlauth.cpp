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
	AuthQuery(Module* me, const std::string& q, const std::string& u, LocalIntExt& e, bool v)
		: SQLQuery(me, q), uid(u), pendingExt(e), verbose(v)
	{
		ServerInstance->Logs->Log("m_sqlauth",DEBUG, "SQLAUTH: query=\"%s\"", q.c_str());
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
	ModuleSQLAuth() : pendingExt("sqlauth-wait", this), SQL(this, "SQL")
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
		ConfigReader Conf;

		SQL.SetProvider("SQL/" + Conf.ReadValue("sqlauth", "dbid", 0));		/* Database ID, given to the SQL service provider */
		freeformquery	= Conf.ReadValue("sqlauth", "query", 0);	/* Field name where username can be found */
		killreason	= Conf.ReadValue("sqlauth", "killreason", 0);	/* Reason to give when access is denied to a user (put your reg details here) */
		allowpattern	= Conf.ReadValue("sqlauth", "allowpattern",0 );	/* Allow nicks matching this pattern without requiring auth */
		verbose		= Conf.ReadFlag("sqlauth", "verbose", 0);		/* Set to true if failed connects should be reported to operators */
		SQL.lookup();
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

		SQL->submit(new AuthQuery(this, SQL->FormatQuery(freeformquery, userinfo), user->uuid, pendingExt, verbose));

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
		return Version("Allow/Deny connections based upon an arbitary SQL table", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSQLAuth)
