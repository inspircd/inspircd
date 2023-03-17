/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

struct CustomVhost {
    const std::string name;
    const std::string password;
    const std::string hash;
    const std::string vhost;

    CustomVhost(const std::string& n, const std::string& p, const std::string& h,
                const std::string& v)
        : name(n)
        , password(p)
        , hash(h)
        , vhost(v) {
    }

    bool CheckPass(User* user, const std::string& pass) const {
        return ServerInstance->PassCompare(user, password, pass, hash);
    }
};

typedef std::multimap<std::string, CustomVhost> CustomVhostMap;
typedef std::pair<CustomVhostMap::iterator, CustomVhostMap::iterator>
MatchingConfigs;

/** Handle /VHOST
 */
class CommandVhost : public Command {
  public:
    CustomVhostMap vhosts;

    CommandVhost(Module* Creator)
        : Command(Creator, "VHOST", 2) {
        syntax = "<username> <password>";
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        MatchingConfigs matching = vhosts.equal_range(parameters[0]);

        for (MatchingConfigs::first_type i = matching.first; i != matching.second; ++i) {
            CustomVhost config = i->second;
            if (config.CheckPass(user, parameters[1])) {
                user->WriteNotice("Setting your VHost: " + config.vhost);
                user->ChangeDisplayedHost(config.vhost);
                return CMD_SUCCESS;
            }
        }

        user->WriteNotice("Invalid username or password.");
        return CMD_FAILURE;
    }
};

class ModuleVHost : public Module {
    CommandVhost cmd;

  public:
    ModuleVHost()
        : cmd(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        CustomVhostMap newhosts;
        ConfigTagList tags = ServerInstance->Config->ConfTags("vhost");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;
            std::string mask = tag->getString("host");
            if (mask.empty()) {
                throw ModuleException("<vhost:host> is empty! at " + tag->getTagLocation());
            }

            std::string username = tag->getString("user");
            if (username.empty()) {
                throw ModuleException("<vhost:user> is empty! at " + tag->getTagLocation());
            }

            std::string pass = tag->getString("pass");
            if (pass.empty()) {
                throw ModuleException("<vhost:pass> is empty! at " + tag->getTagLocation());
            }

            const std::string hash = tag->getString("hash", "plaintext", 1);
            if (stdalgo::string::equalsci(hash, "plaintext")) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "<vhost> tag for %s at %s contains an plain text password, this is insecure!",
                                          username.c_str(), tag->getTagLocation().c_str());
            }

            CustomVhost vhost(username, pass, hash, mask);
            newhosts.insert(std::make_pair(username, vhost));
        }

        cmd.vhosts.swap(newhosts);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to define accounts which can grant a custom virtual host.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleVHost)
