/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015-2016, 2018 Attila Molnar <attilamolnar@hush.com>
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
#include "modules/reload.h"
#include "modules/cap.h"

enum {
    // From IRCv3 capability-negotiation-3.1.
    ERR_INVALIDCAPCMD = 410
};

namespace Cap {
class ManagerImpl;
}

static Cap::ManagerImpl* managerimpl;

class Cap::ManagerImpl : public Cap::Manager,
    public ReloadModule::EventListener {
    /** Stores the cap state of a module being reloaded
     */
    struct CapModData {
        struct Data {
            std::string name;
            std::vector<std::string> users;

            Data(Capability* cap)
                : name(cap->GetName()) {
            }
        };
        std::vector<Data> caps;
    };

    typedef insp::flat_map<std::string, Capability*, irc::insensitive_swo> CapMap;

    ExtItem capext;
    CapMap caps;
    Events::ModuleEventProvider& evprov;

    static bool CanRequest(LocalUser* user, Ext usercaps, Capability* cap,
                           bool adding) {
        const bool hascap = ((usercaps & cap->GetMask()) != 0);
        if (hascap == adding) {
            return true;
        }

        return cap->OnRequest(user, adding);
    }

    Capability::Bit AllocateBit() const {
        Capability::Bit used = 0;
        for (CapMap::const_iterator i = caps.begin(); i != caps.end(); ++i) {
            Capability* cap = i->second;
            used |= cap->GetMask();
        }

        for (size_t i = 0; i < MAX_CAPS; i++) {
            Capability::Bit bit = (static_cast<Capability::Bit>(1) << i);
            if (!(used & bit)) {
                return bit;
            }
        }
        throw ModuleException("Too many caps");
    }

    void OnReloadModuleSave(Module* mod,
                            ReloadModule::CustomData& cd) CXX11_OVERRIDE {
        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "OnReloadModuleSave()");
        if (mod == creator) {
            return;
        }

        CapModData* capmoddata = new CapModData;
        cd.add(this, capmoddata);

        for (CapMap::iterator i = caps.begin(); i != caps.end(); ++i) {
            Capability* cap = i->second;
            // Only save users of caps that belong to the module being reloaded
            if (cap->creator != mod) {
                continue;
            }

            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "Module being reloaded implements cap %s, saving cap users",
                                      cap->GetName().c_str());
            capmoddata->caps.push_back(CapModData::Data(cap));
            CapModData::Data& capdata = capmoddata->caps.back();

            // Populate list with uuids of users who are using the cap
            const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
            for (UserManager::LocalList::const_iterator j = list.begin(); j != list.end();
                    ++j) {
                LocalUser* user = *j;
                if (cap->get(user)) {
                    capdata.users.push_back(user->uuid);
                }
            }
        }
    }

    void OnReloadModuleRestore(Module* mod, void* data) CXX11_OVERRIDE {
        CapModData* capmoddata = static_cast<CapModData*>(data);
        for (std::vector<CapModData::Data>::const_iterator i = capmoddata->caps.begin(); i != capmoddata->caps.end(); ++i) {
            const CapModData::Data& capdata = *i;
            Capability* cap = ManagerImpl::Find(capdata.name);
            if (!cap) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                          "Cap %s is no longer available after reload", capdata.name.c_str());
                continue;
            }

            // Set back the cap for all users who were using it before the reload
            for (std::vector<std::string>::const_iterator j = capdata.users.begin();
                    j != capdata.users.end(); ++j) {
                const std::string& uuid = *j;
                User* user = ServerInstance->FindUUID(uuid);
                if (!user) {
                    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                              "User %s is gone when trying to restore cap %s", uuid.c_str(),
                                              capdata.name.c_str());
                    continue;
                }

                cap->set(user, true);
            }
        }
        delete capmoddata;
    }

  public:
    ManagerImpl(Module* mod, Events::ModuleEventProvider& evprovref)
        : Cap::Manager(mod)
        , ReloadModule::EventListener(mod)
        , capext(mod)
        , evprov(evprovref) {
        managerimpl = this;
    }

    ~ManagerImpl() {
        for (CapMap::iterator i = caps.begin(); i != caps.end(); ++i) {
            Capability* cap = i->second;
            cap->Unregister();
        }
    }

    void AddCap(Cap::Capability* cap) CXX11_OVERRIDE {
        // No-op if the cap is already registered.
        // This allows modules to call SetActive() on a cap without checking if it's active first.
        if (cap->IsRegistered()) {
            return;
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Registering cap %s", cap->GetName().c_str());
        cap->bit = AllocateBit();
        cap->extitem = &capext;
        caps.insert(std::make_pair(cap->GetName(), cap));
        ServerInstance->Modules.AddReferent("cap/" + cap->GetName(), cap);

        FOREACH_MOD_CUSTOM(evprov, Cap::EventListener, OnCapAddDel, (cap, true));
    }

    void DelCap(Cap::Capability* cap) CXX11_OVERRIDE {
        // No-op if the cap is not registered, see AddCap() above
        if (!cap->IsRegistered()) {
            return;
        }

        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Unregistering cap %s", cap->GetName().c_str());

        // Fire the event first so modules can still see who is using the cap which is being unregistered
        FOREACH_MOD_CUSTOM(evprov, Cap::EventListener, OnCapAddDel, (cap, false));

        // Turn off the cap for all users
        const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
        for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i) {
            LocalUser* user = *i;
            cap->set(user, false);
        }

        ServerInstance->Modules.DelReferent(cap);
        cap->Unregister();
        caps.erase(cap->GetName());
    }

    Capability* Find(const std::string& capname) const CXX11_OVERRIDE {
        CapMap::const_iterator it = caps.find(capname);
        if (it != caps.end()) {
            return it->second;
        }
        return NULL;
    }

    void NotifyValueChange(Capability* cap) CXX11_OVERRIDE {
        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Cap %s changed value", cap->GetName().c_str());
        FOREACH_MOD_CUSTOM(evprov, Cap::EventListener, OnCapValueChange, (cap));
    }

    Protocol GetProtocol(LocalUser* user) const {
        return ((capext.get(user) & CAP_302_BIT) ? CAP_302 : CAP_LEGACY);
    }

    void Set302Protocol(LocalUser* user) {
        capext.set(user, capext.get(user) | CAP_302_BIT);
    }

    bool HandleReq(LocalUser* user, const std::string& reqlist) {
        Ext usercaps = capext.get(user);
        irc::spacesepstream ss(reqlist);
        for (std::string capname; ss.GetToken(capname); ) {
            bool remove = (capname[0] == '-');
            if (remove) {
                capname.erase(capname.begin());
            }

            Capability* cap = ManagerImpl::Find(capname);
            if ((!cap) || (!CanRequest(user, usercaps, cap, !remove))) {
                return false;
            }

            if (remove) {
                usercaps = cap->DelFromMask(usercaps);
            } else {
                usercaps = cap->AddToMask(usercaps);
            }
        }

        capext.set(user, usercaps);
        return true;
    }

    void HandleList(std::vector<std::string>& out, LocalUser* user, bool show_all,
                    bool show_values, bool minus_prefix = false) const {
        Ext show_caps = (show_all ? ~0 : capext.get(user));

        for (CapMap::const_iterator i = caps.begin(); i != caps.end(); ++i) {
            Capability* cap = i->second;
            if (!(show_caps & cap->GetMask())) {
                continue;
            }

            if ((show_all) && (!cap->OnList(user))) {
                continue;
            }

            std::string token;
            if (minus_prefix) {
                token.push_back('-');
            }
            token.append(cap->GetName());

            if (show_values) {
                const std::string* capvalue = cap->GetValue(user);
                if ((capvalue) && (!capvalue->empty())
                        && (capvalue->find(' ') == std::string::npos)) {
                    token.push_back('=');
                    token.append(*capvalue, 0, MAX_VALUE_LENGTH);
                }
            }
            out.push_back(token);
        }
    }

    void HandleClear(LocalUser* user, std::vector<std::string>& result) {
        HandleList(result, user, false, false, true);
        capext.unset(user);
    }
};

namespace {
std::string SerializeCaps(const Extensible* container, void* item, bool human) {
    // XXX: Cast away the const because IS_LOCAL() doesn't handle it
    LocalUser* user = IS_LOCAL(const_cast<User*>(static_cast<const User*>
                               (container)));
    if (!user) {
        return std::string();
    }

    // List requested caps
    std::vector<std::string> result;
    managerimpl->HandleList(result, user, false, false);

    // Serialize cap protocol version. If building a human-readable string append a
    // new token, otherwise append only a single character indicating the version.
    std::string version;
    if (human) {
        version.append("capversion=3.");
    }
    switch (managerimpl->GetProtocol(user)) {
    case Cap::CAP_302:
        version.push_back('2');
        break;
    default:
        version.push_back('1');
        break;
    }
    result.push_back(version);

    return stdalgo::string::join(result, ' ');
}
}

Cap::ExtItem::ExtItem(Module* mod)
    : LocalIntExt("caps", ExtensionItem::EXT_USER, mod) {
}

std::string Cap::ExtItem::ToHuman(const Extensible* container,
                                  void* item) const {
    return SerializeCaps(container, item, true);
}

std::string Cap::ExtItem::ToInternal(const Extensible* container,
                                     void* item) const {
    return SerializeCaps(container, item, false);
}

void Cap::ExtItem::FromInternal(Extensible* container,
                                const std::string& value) {
    LocalUser* user = IS_LOCAL(static_cast<User*>(container));
    if (!user) {
        return;    // Can't happen
    }

    // Process the cap protocol version which is a single character at the end of the serialized string
    const char verchar = *value.rbegin();
    if (verchar == '2') {
        managerimpl->Set302Protocol(user);
    }

    // Remove the version indicator from the string passed to HandleReq
    std::string caplist(value, 0, value.size()-1);
    managerimpl->HandleReq(user, caplist);
}

class CapMessage : public Cap::MessageBase {
  public:
    CapMessage(LocalUser* user, const std::string& subcmd,
               const std::string& result, bool asterisk)
        : Cap::MessageBase(subcmd) {
        SetUser(user);
        if (asterisk) {
            PushParam("*");
        }
        PushParamRef(result);
    }
};

class CommandCap : public SplitCommand {
  private:
    Events::ModuleEventProvider evprov;
    Cap::ManagerImpl manager;
    ClientProtocol::EventProvider protoevprov;

    void DisplayResult(LocalUser* user, const std::string& subcmd,
                       std::vector<std::string> result, bool asterisk) {
        size_t maxline = ServerInstance->Config->Limits.MaxLine -
                         ServerInstance->Config->ServerName.size() - user->nick.length() -
                         subcmd.length() - 11;
        std::string line;
        for (std::vector<std::string>::const_iterator iter = result.begin();
                iter != result.end(); ++iter) {
            if (line.length() + iter->length() < maxline) {
                line.append(*iter);
                line.push_back(' ');
            } else {
                DisplaySingleResult(user, subcmd, line, asterisk);
                line.clear();
            }
        }
        DisplaySingleResult(user, subcmd, line, false);
    }

    void DisplaySingleResult(LocalUser* user, const std::string& subcmd,
                             const std::string& result, bool asterisk) {
        CapMessage msg(user, subcmd, result, asterisk);
        ClientProtocol::Event ev(protoevprov, msg);
        user->Send(ev);
    }

  public:
    LocalIntExt holdext;

    CommandCap(Module* mod)
        : SplitCommand(mod, "CAP", 1)
        , evprov(mod, "event/cap")
        , manager(mod, evprov)
        , protoevprov(mod, name)
        , holdext("cap_hold", ExtensionItem::EXT_USER, mod) {
        works_before_reg = true;
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        if (user->registered != REG_ALL) {
            holdext.set(user, 1);
        }

        const std::string& subcommand = parameters[0];
        if (irc::equals(subcommand, "REQ")) {
            if (parameters.size() < 2) {
                return CMD_FAILURE;
            }

            const std::string replysubcmd = (manager.HandleReq(user,
                                             parameters[1]) ? "ACK" : "NAK");
            DisplaySingleResult(user, replysubcmd, parameters[1], false);
        } else if (irc::equals(subcommand, "END")) {
            holdext.unset(user);
        } else if (irc::equals(subcommand, "LS") || irc::equals(subcommand, "LIST")) {
            Cap::Protocol capversion = Cap::CAP_LEGACY;
            const bool is_ls = (subcommand.length() == 2);
            if ((is_ls) && (parameters.size() > 1)) {
                unsigned int version = ConvToNum<unsigned int>(parameters[1]);
                if (version >= 302) {
                    capversion = Cap::CAP_302;
                    manager.Set302Protocol(user);
                }
            }

            std::vector<std::string> result;
            // Show values only if supports v3.2 and doing LS
            manager.HandleList(result, user, is_ls, ((is_ls)
                               && (capversion != Cap::CAP_LEGACY)));
            DisplayResult(user, subcommand, result, (capversion != Cap::CAP_LEGACY));
        } else if (irc::equals(subcommand, "CLEAR") && (manager.GetProtocol(user) == Cap::CAP_LEGACY)) {
            std::vector<std::string> result;
            manager.HandleClear(user, result);
            DisplayResult(user, "ACK", result, false);
        } else {
            user->WriteNumeric(ERR_INVALIDCAPCMD, subcommand.empty() ? "*" : subcommand,
                               "Invalid CAP subcommand");
            return CMD_FAILURE;
        }

        return CMD_SUCCESS;
    }
};

class PoisonCap : public Cap::Capability {
  public:
    PoisonCap(Module* mod)
        : Cap::Capability(mod, "inspircd.org/poison") {
    }

    bool OnRequest(LocalUser* user, bool adding) CXX11_OVERRIDE {
        // Reject the attempt to enable this capability.
        return false;
    }
};

class ModuleCap : public Module {
  private:
    CommandCap cmd;
    PoisonCap poisoncap;
    Cap::Capability stdrplcap;

  public:
    ModuleCap()
        : cmd(this)
        , poisoncap(this)
        , stdrplcap(this, "inspircd.org/standard-replies") {
    }

    ModResult OnCheckReady(LocalUser* user) CXX11_OVERRIDE {
        return (cmd.holdext.get(user) ? MOD_RES_DENY : MOD_RES_PASSTHRU);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides support for the IRCv3 Client Capability Negotiation extension.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleCap)
