/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "account.h"
#include "sql.h"
#include "hash.h"

/* $ModDesc: Allow/Deny connections based upon an arbitary SQL table */

enum AuthState {
	AUTH_STATE_NONE = 0,
	AUTH_STATE_BUSY = 1,
	AUTH_STATE_FAIL = 2,
	AUTH_STATE_OK   = 3
};

static dynamic_reference<AccountProvider> account("account");

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
		SQLEntries result;
		if (res.GetRow(result))
		{
			std::vector<std::string> cols;
			res.GetCols(cols);
			std::string acct, tag;
			for(unsigned int i=0; i < cols.size(); i++)
			{
				if (result[i].nul)
					continue;
				if (cols[i] == "account")
					acct = result[i].value;
				if (cols[i] == "tag")
					tag = result[i].value;
				if (cols[i] == "class")
					ServerInstance->ForcedClass.set(user, result[i].value);
			}
			if (!acct.empty() && account)
			{
				account->DoLogin(user, acct, tag);
			}
			pendingExt.set(user, AUTH_STATE_OK);
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
	bool verbose;

 public:
	ModuleSQLAuth() : pendingExt(EXTENSIBLE_USER, "sqlauth-wait", this), SQL("SQL")
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(pendingExt);
		Implementation eventlist[] = { I_OnCheckReady, I_OnUserRegister, I_OnSetConnectClass };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void ReadConfig(ConfigReadStatus&)
	{
		ConfigTag* conf = ServerInstance->Config->GetTag("sqlauth");
		std::string dbid = conf->getString("dbid");
		if (dbid.empty())
			SQL.SetProvider("SQL");
		else
			SQL.SetProvider("SQL/" + dbid);
		freeformquery = conf->getString("query");
		verbose = conf->getBool("verbose");
	}

	void OnUserRegister(LocalUser* user)
	{
		// Note this is their initial (unresolved) connect block
		ConfigTag* tag = user->MyClass->config;
		if (!tag->getBool("usesqlauth", true))
			return;

		if (pendingExt.get(user))
			return;

		if (!SQL)
		{
			ServerInstance->SNO->WriteGlobalSno('a', "SQLAUTH: No database present, connection denied");
			pendingExt.set(user, AUTH_STATE_FAIL);
			return;
		}

		pendingExt.set(user, AUTH_STATE_BUSY);

		ParamM userinfo;
		user->PopulateInfoMap(userinfo);
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
		if (pendingExt.get(user) == AUTH_STATE_BUSY)
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		if (myclass->config->getBool("requiresqlauth", false) && pendingExt.get(user) == AUTH_STATE_FAIL)
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Allow/Deny connections based upon an arbitary SQL table", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSQLAuth)
