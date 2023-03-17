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
/// $ModDesc: Allows detection of clients by version string.
/// $ModDepends: core 3

#include "inspircd.h"
#include "modules/regex.h"

enum ClientAction {
    // Kill clients that match the check.
    CA_KILL,

    // Send a NOTICE to clients that match the check.
    CA_NOTICE,

    // Send a PRIVMSG to clients that match the check.
    CA_PRIVMSG
};

struct ClientInfo {
    // The action to take against a client that matches this action.
    ClientAction action;

    // The message to give when performing the action.
    std::string message;

    // A regular expression which matches a client version string.
    Regex* pattern;
};

class ModuleClientCheck : public Module {
  private:
    LocalIntExt ext;
    std::vector<ClientInfo> clients;
    dynamic_reference_nocheck<RegexFactory> rf;
    std::string origin;
    std::string originnick;

  public:
    ModuleClientCheck()
        : ext("checking-client-version", ExtensionItem::EXT_USER, this)
        , rf(this, "regex") {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* clientcheck = ServerInstance->Config->ConfValue("clientcheck");

        const std::string engine = clientcheck->getString("engine");
        dynamic_reference_nocheck<RegexFactory> newrf(this, engine.empty() ? "regex": "regex/" + engine);
        if (!newrf) {
            throw ModuleException("<clientcheck:engine> (" + engine +
                                  ") is not a recognised regex engine.");
        }

        const std::string neworigin = clientcheck->getString("origin", ServerInstance->Config->ServerName);
        if (neworigin.empty() || neworigin.find(' ') != std::string::npos) {
            throw ModuleException("<clientcheck:origin> (" + neworigin +
                                  ") is not a valid nick!user@host mask.");
        }

        std::vector<ClientInfo> newclients;
        ConfigTagList tags = ServerInstance->Config->ConfTags("clientmatch");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;

            ClientInfo ci;
            ci.message = tag->getString("message");

            const std::string actionstr = tag->getString("action", "privmsg", 1);
            if (irc::equals(actionstr, "kill")) {
                ci.action = CA_KILL;
            } else if (irc::equals(actionstr, "notice")) {
                ci.action = CA_NOTICE;
            } else if (irc::equals(actionstr, "privmsg")) {
                ci.action = CA_PRIVMSG;
            } else {
                throw ModuleException("<clientmatch:action>; must be set to one of 'gline', 'kill', 'notice', or 'privmsg'.");
            }

            try {
                ci.pattern = newrf->Create(tag->getString("pattern"));
            } catch (const RegexException& err) {
                throw ModuleException("<clientmatch:pattern> is not a well formed regular expression: "
                                      + err.GetReason());
            }

            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Client check: %s -> %s (%s)",
                                      ci.pattern->GetRegexString().c_str(),
                                      actionstr.c_str(), ci.message.c_str());
            newclients.push_back(ci);
        }

        rf.SetProvider(newrf.GetProvider());
        std::swap(clients, newclients);
        origin = neworigin;
        originnick = neworigin.substr(0, origin.find('!'));
    }

    void OnUserConnect(LocalUser* user) CXX11_OVERRIDE {
        ext.set(user, 1);

        ClientProtocol::Messages::Privmsg msg(origin, user, "\x1VERSION\x1", MSG_PRIVMSG);
        user->Send(ServerInstance->GetRFCEvents().privmsg, msg);
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        if (validated || !ext.get(user) || !rf) {
            return MOD_RES_PASSTHRU;
        }

        if (command != "NOTICE" || parameters.size() < 2) {
            return MOD_RES_PASSTHRU;
        }

        if (parameters[0] != originnick) {
            return MOD_RES_PASSTHRU;
        }

        if (parameters[1].length() < 10 || parameters[1][0] != '\x1') {
            return MOD_RES_PASSTHRU;
        }

        const std::string prefix = parameters[1].substr(0, 9);
        if (!irc::equals(prefix, "\1VERSION ")) {
            return MOD_RES_PASSTHRU;
        }

        size_t msgsize = parameters[1].size();
        size_t lastpos = msgsize - (parameters[1][msgsize - 1] == '\x1' ? 9 : 10);

        const std::string version = parameters[1].substr(9, lastpos);
        for (std::vector<ClientInfo>::const_iterator iter = clients.begin(); iter != clients.end(); ++iter) {
            const ClientInfo& ci = *iter;
            if (!ci.pattern->Matches(version)) {
                continue;
            }

            switch (ci.action) {
            case CA_KILL: {
                ServerInstance->Users->QuitUser(user, ci.message);
                break;
            }
            case CA_NOTICE: {
                ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy,
                                                      origin, user, ci.message, MSG_NOTICE);
                user->Send(ServerInstance->GetRFCEvents().privmsg, msg);
                break;
            }
            case CA_PRIVMSG: {
                ClientProtocol::Messages::Privmsg msg(ClientProtocol::Messages::Privmsg::nocopy,
                                                      origin, user, ci.message, MSG_PRIVMSG);
                user->Send(ServerInstance->GetRFCEvents().privmsg, msg);
                break;
            }
            }
            break;
        }

        ext.unset(user);
        return MOD_RES_DENY;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows detection of clients by version string.");
    }
};

MODULE_INIT(ModuleClientCheck)
