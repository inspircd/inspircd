/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "extension.h"
#include "modules/sql.h"
#include "modules/hash.h"
#include "modules/ssl.h"

enum AuthState {
	AUTH_STATE_NONE = 0,
	AUTH_STATE_BUSY = 1,
	AUTH_STATE_FAIL = 2
};

class AuthQuery final
	: public SQL::Query
{
public:
	const std::string uid;
	IntExtItem& pendingExt;
	bool verbose;
	const std::string& kdf;
	const std::string& pwcolumn;

	AuthQuery(Module* me, const std::string& u, IntExtItem& e, bool v, const std::string& kd, const std::string& pwcol)
		: SQL::Query(me)
		, uid(u)
		, pendingExt(e)
		, verbose(v)
		, kdf(kd)
		, pwcolumn(pwcol)
	{
	}

	void OnResult(SQL::Result& res) override
	{
		LocalUser* user = ServerInstance->Users.FindUUID<LocalUser>(uid);
		if (!user)
			return;

		if (res.Rows())
		{
			if (!kdf.empty())
			{
				HashProvider* hashprov = ServerInstance->Modules.FindDataService<HashProvider>("hash/" + kdf);
				if (!hashprov)
				{
					if (verbose)
						ServerInstance->SNO.WriteGlobalSno('a', "Forbidden connection from {} (a provider for {} was not loaded)", user->GetRealMask(), kdf);
					pendingExt.Set(user, AUTH_STATE_FAIL);
					return;
				}

				size_t colindex = 0;
				if (!pwcolumn.empty() && !res.HasColumn(pwcolumn, colindex))
				{
					if (verbose)
						ServerInstance->SNO.WriteGlobalSno('a', "Forbidden connection from {} (the column specified ({}) was not returned)", user->GetRealMask(), pwcolumn);
					pendingExt.Set(user, AUTH_STATE_FAIL);
					return;
				}

				SQL::Row row;
				while (res.GetRow(row))
				{
					if (row[colindex].has_value() && hashprov->Compare(user->password, *row[colindex]))
					{
						pendingExt.Set(user, AUTH_STATE_NONE);
						return;
					}
				}

				if (verbose)
					ServerInstance->SNO.WriteGlobalSno('a', "Forbidden connection from {} (password from the SQL query did not match the user provided password)", user->GetRealMask());
				pendingExt.Set(user, AUTH_STATE_FAIL);
				return;
			}

			pendingExt.Set(user, AUTH_STATE_NONE);
		}
		else
		{
			if (verbose)
				ServerInstance->SNO.WriteGlobalSno('a', "Forbidden connection from {} (SQL query returned no matches)", user->GetRealMask());
			pendingExt.Set(user, AUTH_STATE_FAIL);
		}
	}

	void OnError(const SQL::Error& error) override
	{
		auto* user = ServerInstance->Users.Find(uid);
		if (!user)
			return;
		pendingExt.Set(user, AUTH_STATE_FAIL);
		if (verbose)
			ServerInstance->SNO.WriteGlobalSno('a', "Forbidden connection from {} (SQL query failed: {})", user->GetRealMask(), error.ToString());
	}
};

class ModuleSQLAuth final
	: public Module
{
	IntExtItem pendingExt;
	dynamic_reference<SQL::Provider> SQL;
	UserCertificateAPI sslapi;

	std::string freeformquery;
	std::string killreason;
	std::vector<std::string> exemptions;
	bool verbose;
	std::vector<std::string> hash_algos;
	std::string kdf;
	std::string pwcolumn;

public:
	ModuleSQLAuth()
		: Module(VF_VENDOR, "Allows connecting users to be authenticated against an arbitrary SQL table.")
		, pendingExt(this, "sqlauth-wait", ExtensionType::USER)
		, SQL(this, "SQL")
		, sslapi(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& conf = ServerInstance->Config->ConfValue("sqlauth");
		std::string dbid = conf->getString("dbid");
		if (dbid.empty())
			SQL.SetProvider("SQL");
		else
			SQL.SetProvider("SQL/" + dbid);
		freeformquery = conf->getString("query");
		killreason = conf->getString("killreason");
		verbose = conf->getBool("verbose");
		kdf = conf->getString("kdf");
		pwcolumn = conf->getString("column");

		exemptions.clear();
		for (const auto& [_, etag] : ServerInstance->Config->ConfTags("sqlexemption"))
		{
			const std::string mask = etag->getString("mask");
			if (!mask.empty())
				exemptions.push_back(mask);
		}

		// Begin v3 config compatibility.
		const std::string allowpattern = conf->getString("allowpattern");
		if (!allowpattern.empty())
			exemptions.push_back(allowpattern + "!*@*");
		// End v3 config compatibility.

		hash_algos.clear();
		irc::commasepstream algos(conf->getString("hash", "md5,sha256"));
		std::string algo;
		while (algos.GetToken(algo))
			hash_algos.push_back(algo);
	}

	ModResult OnUserRegister(LocalUser* user) override
	{
		// Note this is their initial (unresolved) connect block
		if (!user->GetClass()->config->getBool("usesqlauth", true))
			return MOD_RES_PASSTHRU;

		for (const auto& exemption : exemptions)
		{
			if (InspIRCd::MatchCIDR(user->GetRealMask(), exemption) || InspIRCd::MatchCIDR(user->GetMask(), exemption))
				return MOD_RES_PASSTHRU;
		}

		if (pendingExt.Get(user))
			return MOD_RES_PASSTHRU;

		if (!SQL)
		{
			ServerInstance->SNO.WriteGlobalSno('a', "Forbidden connection from {} (SQL database not present)", user->GetRealMask());
			ServerInstance->Users.QuitUser(user, killreason);
			return MOD_RES_PASSTHRU;
		}

		pendingExt.Set(user, AUTH_STATE_BUSY);

		SQL::ParamMap userinfo;
		SQL::PopulateUserInfo(user, userinfo);
		userinfo["pass"] = user->password;
		userinfo["sslfp"] = sslapi ? sslapi->GetFingerprint(user) : "";

		for (const auto& algo : hash_algos)
		{
			HashProvider* hashprov = ServerInstance->Modules.FindDataService<HashProvider>("hash/" + algo);
			if (hashprov && !hashprov->IsKDF())
				userinfo[algo + "pass"] = hashprov->Generate(user->password);
		}

		SQL->Submit(new AuthQuery(this, user->uuid, pendingExt, verbose, kdf, pwcolumn), freeformquery, userinfo);

		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckReady(LocalUser* user) override
	{
		switch (pendingExt.Get(user))
		{
			case AUTH_STATE_NONE:
				return MOD_RES_PASSTHRU;
			case AUTH_STATE_BUSY:
				return MOD_RES_DENY;
			case AUTH_STATE_FAIL:
				ServerInstance->Users.QuitUser(user, killreason);
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleSQLAuth)
