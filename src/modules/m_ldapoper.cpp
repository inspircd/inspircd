/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Christos Triantafyllidis
 *   Copyright (C) 2018-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013-2014 Adam <Adam@anope.org>
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
#include "modules/ldap.h"

namespace {
Module* me;
}

class LDAPOperBase : public LDAPInterface {
  protected:
    const std::string uid;
    const std::string opername;
    const std::string password;

    void Fallback(User* user) {
        if (!user) {
            return;
        }

        Command* oper_command = ServerInstance->Parser.GetHandler("OPER");
        if (!oper_command) {
            return;
        }

        CommandBase::Params params;
        params.push_back(opername);
        params.push_back(password);
        ClientProtocol::TagMap tags;
        oper_command->Handle(user, CommandBase::Params(params, tags));
    }

    void Fallback() {
        User* user = ServerInstance->FindUUID(uid);
        Fallback(user);
    }

  public:
    LDAPOperBase(Module* mod, const std::string& uuid, const std::string& oper,
                 const std::string& pass)
        : LDAPInterface(mod)
        , uid(uuid), opername(oper), password(pass) {
    }

    void OnError(const LDAPResult& err) CXX11_OVERRIDE {
        ServerInstance->SNO->WriteToSnoMask('a', "Error searching LDAP server: %s", err.getError().c_str());
        Fallback();
        delete this;
    }
};

class BindInterface : public LDAPOperBase {
  public:
    BindInterface(Module* mod, const std::string& uuid, const std::string& oper,
                  const std::string& pass)
        : LDAPOperBase(mod, uuid, oper, pass) {
    }

    void OnResult(const LDAPResult& r) CXX11_OVERRIDE {
        User* user = ServerInstance->FindUUID(uid);
        ServerConfig::OperIndex::const_iterator iter = ServerInstance->Config->oper_blocks.find(opername);

        if (!user || iter == ServerInstance->Config->oper_blocks.end()) {
            Fallback();
            delete this;
            return;
        }

        OperInfo* ifo = iter->second;
        user->Oper(ifo);
        delete this;
    }
};

class SearchInterface : public LDAPOperBase {
    const std::string provider;

    bool HandleResult(const LDAPResult& result) {
        dynamic_reference<LDAPProvider> LDAP(me, provider);
        if (!LDAP || result.empty()) {
            return false;
        }

        try {
            const LDAPAttributes& attr = result.get(0);
            std::string bindDn = attr.get("dn");
            if (bindDn.empty()) {
                return false;
            }

            LDAP->Bind(new BindInterface(this->creator, uid, opername, password), bindDn,
                       password);
        } catch (LDAPException& ex) {
            ServerInstance->SNO->WriteToSnoMask('a',
                                                "Error searching LDAP server: " + ex.GetReason());
        }

        return true;
    }

  public:
    SearchInterface(Module* mod, const std::string& prov, const std::string &uuid,
                    const std::string& oper, const std::string& pass)
        : LDAPOperBase(mod, uuid, oper, pass)
        , provider(prov) {
    }

    void OnResult(const LDAPResult& result) CXX11_OVERRIDE {
        if (!HandleResult(result)) {
            Fallback();
        }
        delete this;
    }
};

class AdminBindInterface : public LDAPInterface {
    const std::string provider;
    const std::string user;
    const std::string opername;
    const std::string password;
    const std::string base;
    const std::string what;

  public:
    AdminBindInterface(Module* c, const std::string& p, const std::string& u,
                       const std::string& o, const std::string& pa, const std::string& b,
                       const std::string& w)
        : LDAPInterface(c)
        , provider(p)
        , user(u)
        , opername(o)
        , password(pa)
        , base(b)
        , what(w) {
    }

    void OnResult(const LDAPResult& r) CXX11_OVERRIDE {
        dynamic_reference<LDAPProvider> LDAP(me, provider);
        if (LDAP) {
            try {
                LDAP->Search(new SearchInterface(this->creator, provider, user, opername,
                                                 password), base, what);
            } catch (LDAPException& ex) {
                ServerInstance->SNO->WriteToSnoMask('a',
                                                    "Error searching LDAP server: " + ex.GetReason());
            }
        }
        delete this;
    }

    void OnError(const LDAPResult& err) CXX11_OVERRIDE {
        ServerInstance->SNO->WriteToSnoMask('a', "Error binding as manager to LDAP server: " + err.getError());
        delete this;
    }
};

class ModuleLDAPOper : public Module {
    dynamic_reference<LDAPProvider> LDAP;
    std::string base;
    std::string attribute;

  public:
    ModuleLDAPOper()
        : LDAP(this, "LDAP") {
        me = this;
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("ldapoper");

        LDAP.SetProvider("LDAP/" + tag->getString("dbid"));
        base = tag->getString("baserdn");
        attribute = tag->getString("attribute");
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        if (validated && command == "OPER" && parameters.size() >= 2) {
            const std::string& opername = parameters[0];
            const std::string& password = parameters[1];

            ServerConfig::OperIndex::const_iterator it =
            ServerInstance->Config->oper_blocks.find(opername);
            if (it == ServerInstance->Config->oper_blocks.end()) {
                return MOD_RES_PASSTHRU;
            }

            ConfigTag* tag = it->second->oper_block;
            if (!tag) {
                return MOD_RES_PASSTHRU;
            }

            std::string acceptedhosts = tag->getString("host");
            if (!InspIRCd::MatchMask(acceptedhosts, user->MakeHost(), user->MakeHostIP())) {
                return MOD_RES_PASSTHRU;
            }

            if (!LDAP) {
                return MOD_RES_PASSTHRU;
            }

            try {
                std::string what = attribute + "=" + opername;
                LDAP->BindAsManager(new AdminBindInterface(this, LDAP.GetProvider(), user->uuid,
                                    opername, password, base, what));
                return MOD_RES_DENY;
            } catch (LDAPException& ex) {
                ServerInstance->SNO->WriteToSnoMask('a', "LDAP exception: " + ex.GetReason());
            }
        }

        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows server operators to be authenticated against an LDAP database.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleLDAPOper)
