/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Allows migrating a live network which is using RFC 1459 casemapping to ASCII casemapping.


#include "inspircd.h"
#include "modules/ircv3_replies.h"

class CommandASCIICheck
    : public SplitCommand {
  private:
    static size_t Hash(const std::string& str) {
        // Stolen from irc::insensitive::operator()
        size_t hash = 0;
        for (std::string::const_iterator chr = str.begin(); chr != str.end(); ++chr) {
            hash = 5 * hash + ascii_case_insensitive_map[(unsigned char)*chr];
        }
        return hash;
    }

  public:
    CommandASCIICheck(Module* Creator)
        : SplitCommand(Creator, "ASCIICHECK") {
        allow_empty_last_param = false;
        flags_needed = 'o';
        Penalty = 10;
    }

    CmdResult HandleLocal(LocalUser* user,
                          const Params& parameters) CXX11_OVERRIDE {
        if (ServerInstance->Config->CaseMapping != "rfc1459") {
            user->WriteNotice("*** ASCIICHECK: This server is not using RFC 1459 casemapping.");
            return CMD_FAILURE;
        }

        size_t chans = 0;
        for (chan_hash::iterator i = ServerInstance->chanlist.begin(); i != ServerInstance->chanlist.end(); ++i) {
            size_t oldhash = Hash(i->first);
            size_t newhash = irc::insensitive()(i->first);
            if (oldhash == newhash) {
                continue;
            }

            user->WriteNotice(
                InspIRCd::Format("*** ASCIICHECK: The channel hashcode for %s will change from %lu to %lu",
                                 i->first.c_str(), oldhash, newhash));
            chans++;
        }

        size_t users = 0;
        for (user_hash::iterator i = ServerInstance->Users.clientlist.begin(); i != ServerInstance->Users.clientlist.end(); ++i) {
            size_t oldhash = Hash(i->first);
            size_t newhash = irc::insensitive()(i->first);
            if (oldhash == newhash) {
                continue;
            }

            user->WriteNotice(
                InspIRCd::Format("*** ASCIICHECK: The user hashcode for %s will change from %lu to %lu",
                                 i->first.c_str(), oldhash, newhash));
            users++;
        }

        user->WriteNotice(InspIRCd::Format("*** ASCIICHECK: Check complete: %lu/%lu channels and %lu/%lu users need to be rehashed.",
                                           chans, ServerInstance->chanlist.size(), users, ServerInstance->Users->clientlist.size()));
        return CMD_SUCCESS;
    }
};

class ModuleASCIISwitch : public Module {
  private:
    CommandASCIICheck cmd;

    template <typename T>
    void RehashHashmap(T& hashmap) {
        // Stolen from the codepage module
        T newhash(hashmap.bucket_count());
        for (typename T::const_iterator i = hashmap.begin(); i != hashmap.end(); ++i) {
            newhash.insert(std::make_pair(i->first, i->second));
        }
        hashmap.swap(newhash);
    }

  public:
    ModuleASCIISwitch()
        : cmd(this) {
    }

    void OnModuleRehash(User* user, const std::string& param) CXX11_OVERRIDE {
        if (!irc::equals(param, "ascii")) {
            return;
        }

        if (ServerInstance->Config->CaseMapping != "rfc1459") {
            const std::string message =
            "Unable to migrate; your server is not using RFC 1459 casemapping!";
            ServerInstance->Logs.Log(MODNAME, LOG_DEFAULT, message);
            if (user) {
                user->WriteRemoteNotice("*** ASCIISWITCH: " + message);
            }
            return;
        }

        // Apply the new casemap.
        ServerInstance->Config->CaseMapping = "ascii";
        national_case_insensitive_map = ascii_case_insensitive_map;

        // Rehash any dependent hashmaps.
        RehashHashmap(ServerInstance->Users.clientlist);
        RehashHashmap(ServerInstance->Users.uuidlist);
        RehashHashmap(ServerInstance->chanlist);

        // Regenerate 005 and update clients on the change.
        ServerInstance->ISupport.Build();
        const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
        for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i) {
            LocalUser* luser = *i;
            ServerInstance->ISupport.SendTo(luser);
        }

        // Ask modules to reload their config so they can rehash their hashmaps too.
        ConfigStatus status(user, false);
        const ModuleManager::ModuleMap& mods = ServerInstance->Modules->GetModules();
        for (ModuleManager::ModuleMap::const_iterator i = mods.begin(); i != mods.end(); ++i) {
            try {
                ServerInstance->Logs->Log("MODULE", LOG_DEBUG, "Rehashing " + i->first);
                i->second->ReadConfig(status);
            } catch (CoreException& modex) {
                ServerInstance->Logs->Log("MODULE", LOG_DEFAULT,
                                          "Exception caught: " + modex.GetReason());
                if (user) {
                    user->WriteNotice("*** ASCIISWITCH: " + i->first + ": " + modex.GetReason());
                }
            }
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows migrating a live network which is using RFC 1459 casemapping to ASCII casemapping.");
    }
};

MODULE_INIT(ModuleASCIISwitch)
