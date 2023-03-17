/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013, 2017-2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007-2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/ssl.h"

enum AuthState {
    AUTH_STATE_NONE = 0,
    AUTH_STATE_BUSY = 1,
    AUTH_STATE_FAIL = 2
};

class AuthQuery : public SQL::Query {
  public:
    const std::string uid;
    LocalIntExt& pendingExt;
    bool verbose;
    const std::string& kdf;
    const std::string& pwcolumn;

    AuthQuery(Module* me, const std::string& u, LocalIntExt& e, bool v,
              const std::string& kd, const std::string& pwcol)
        : SQL::Query(me)
        , uid(u)
        , pendingExt(e)
        , verbose(v)
        , kdf(kd)
        , pwcolumn(pwcol) {
    }

    void OnResult(SQL::Result& res) CXX11_OVERRIDE {
        LocalUser* user = IS_LOCAL(ServerInstance->FindUUID(uid));
        if (!user) {
            return;
        }

        if (res.Rows()) {
            if (!kdf.empty()) {
                HashProvider* hashprov =
                ServerInstance->Modules->FindDataService<HashProvider>("hash/" + kdf);
                if (!hashprov) {
                    if (verbose) {
                        ServerInstance->SNO->WriteGlobalSno('a',
                                                            "Forbidden connection from %s (a provider for %s was not loaded)",
                                                            user->GetFullRealHost().c_str(), kdf.c_str());
                    }
                    pendingExt.set(user, AUTH_STATE_FAIL);
                    return;
                }

                size_t colindex = 0;
                if (!pwcolumn.empty() && !res.HasColumn(pwcolumn, colindex)) {
                    if (verbose) {
                        ServerInstance->SNO->WriteGlobalSno('a',
                                                            "Forbidden connection from %s (the column specified (%s) was not returned)",
                                                            user->GetFullRealHost().c_str(), pwcolumn.c_str());
                    }
                    pendingExt.set(user, AUTH_STATE_FAIL);
                    return;
                }

                SQL::Row row;
                while (res.GetRow(row)) {
                    if (hashprov->Compare(user->password, row[colindex])) {
                        pendingExt.set(user, AUTH_STATE_NONE);
                        return;
                    }
                }

                if (verbose) {
                    ServerInstance->SNO->WriteGlobalSno('a',
                                                        "Forbidden connection from %s (password from the SQL query did not match the user provided password)",
                                                        user->GetFullRealHost().c_str());
                }
                pendingExt.set(user, AUTH_STATE_FAIL);
                return;
            }

            pendingExt.set(user, AUTH_STATE_NONE);
        } else {
            if (verbose) {
                ServerInstance->SNO->WriteGlobalSno('a',
                                                    "Forbidden connection from %s (SQL query returned no matches)",
                                                    user->GetFullRealHost().c_str());
            }
            pendingExt.set(user, AUTH_STATE_FAIL);
        }
    }

    void OnError(SQL::Error& error) CXX11_OVERRIDE {
        User* user = ServerInstance->FindNick(uid);
        if (!user) {
            return;
        }
        pendingExt.set(user, AUTH_STATE_FAIL);
        if (verbose) {
            ServerInstance->SNO->WriteGlobalSno('a',
                                                "Forbidden connection from %s (SQL query failed: %s)",
                                                user->GetFullRealHost().c_str(), error.ToString());
        }
    }
};

class ModuleSQLAuth : public Module {
    LocalIntExt pendingExt;
    dynamic_reference<SQL::Provider> SQL;
    UserCertificateAPI sslapi;

    std::string freeformquery;
    std::string killreason;
    std::string allowpattern;
    bool verbose;
    std::vector<std::string> hash_algos;
    std::string kdf;
    std::string pwcolumn;

  public:
    ModuleSQLAuth()
        : pendingExt("sqlauth-wait", ExtensionItem::EXT_USER, this)
        , SQL(this, "SQL")
        , sslapi(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* conf = ServerInstance->Config->ConfValue("sqlauth");
        std::string dbid = conf->getString("dbid");
        if (dbid.empty()) {
            SQL.SetProvider("SQL");
        } else {
            SQL.SetProvider("SQL/" + dbid);
        }
        freeformquery = conf->getString("query");
        killreason = conf->getString("killreason");
        allowpattern = conf->getString("allowpattern");
        verbose = conf->getBool("verbose");
        kdf = conf->getString("kdf");
        pwcolumn = conf->getString("column");

        hash_algos.clear();
        irc::commasepstream algos(conf->getString("hash", "md5,sha256"));
        std::string algo;
        while (algos.GetToken(algo)) {
            hash_algos.push_back(algo);
        }
    }

    ModResult OnUserRegister(LocalUser* user) CXX11_OVERRIDE {
        // Note this is their initial (unresolved) connect block
        ConfigTag* tag = user->MyClass->config;
        if (!tag->getBool("usesqlauth", true)) {
            return MOD_RES_PASSTHRU;
        }

        if (!allowpattern.empty() && InspIRCd::Match(user->nick,allowpattern)) {
            return MOD_RES_PASSTHRU;
        }

        if (pendingExt.get(user)) {
            return MOD_RES_PASSTHRU;
        }

        if (!SQL) {
            ServerInstance->SNO->WriteGlobalSno('a',
                                                "Forbidden connection from %s (SQL database not present)",
                                                user->GetFullRealHost().c_str());
            ServerInstance->Users->QuitUser(user, killreason);
            return MOD_RES_PASSTHRU;
        }

        pendingExt.set(user, AUTH_STATE_BUSY);

        SQL::ParamMap userinfo;
        SQL::PopulateUserInfo(user, userinfo);
        userinfo["pass"] = user->password;
        userinfo["sslfp"] = sslapi ? sslapi->GetFingerprint(user) : "";
        userinfo["certfp"] = userinfo["sslfp"]; // deprecated


        for (std::vector<std::string>::const_iterator it = hash_algos.begin(); it != hash_algos.end(); ++it) {
            HashProvider* hashprov =
            ServerInstance->Modules->FindDataService<HashProvider>("hash/" + *it);
            if (hashprov && !hashprov->IsKDF()) {
                userinfo[*it + "pass"] = hashprov->Generate(user->password);
            }
        }

        SQL->Submit(new AuthQuery(this, user->uuid, pendingExt, verbose, kdf, pwcolumn), freeformquery, userinfo);

        return MOD_RES_PASSTHRU;
    }

    ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE {
        switch (pendingExt.get(user)) {
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

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows connecting users to be authenticated against an arbitrary SQL table.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleSQLAuth)
