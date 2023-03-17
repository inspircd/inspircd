/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014-2016 Attila Molnar <attilamolnar@hush.com>
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

enum {
    // InspIRCd-specific.
    ERR_TOPICLOCK = 744
};

class CommandSVSTOPIC : public Command {
  public:
    CommandSVSTOPIC(Module* Creator)
        : Command(Creator, "SVSTOPIC", 1, 4) {
        flags_needed = FLAG_SERVERONLY;
    }

    CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE {
        if (!user->server->IsULine()) {
            // Ulines only
            return CMD_FAILURE;
        }

        Channel* chan = ServerInstance->FindChan(parameters[0]);
        if (!chan) {
            return CMD_FAILURE;
        }

        if (parameters.size() == 4) {
            // 4 parameter version, set all topic data on the channel to the ones given in the parameters
            time_t topicts = ConvToNum<time_t>(parameters[1]);
            if (!topicts) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "Received SVSTOPIC with a 0 topicts, dropped.");
                return CMD_INVALID;
            }

            chan->SetTopic(user, parameters[3], topicts, &parameters[2]);
        } else {
            // 1 parameter version, nuke the topic
            chan->SetTopic(user, std::string(), 0);
            chan->setby.clear();
        }

        return CMD_SUCCESS;
    }

    RouteDescriptor GetRouting(User* user,
                               const Params& parameters) CXX11_OVERRIDE {
        return ROUTE_BROADCAST;
    }
};

// TODO: add a BoolExtItem to replace this.
class FlagExtItem : public ExtensionItem {
  public:
    FlagExtItem(const std::string& key, Module* owner)
        : ExtensionItem(key, ExtensionItem::EXT_CHANNEL, owner) {
    }

    bool get(const Extensible* container) const {
        return (get_raw(container) != NULL);
    }

    std::string ToHuman(const Extensible* container,
                        void* item) const CXX11_OVERRIDE {
        // Make the human version more readable.
        return "true";
    }

    std::string ToNetwork(const Extensible* container,
                          void* item) const CXX11_OVERRIDE {
        return "1";
    }

    void FromNetwork(Extensible* container,
                     const std::string& value) CXX11_OVERRIDE {
        if (value == "1") {
            set_raw(container, this);
        } else {
            unset_raw(container);
        }
    }

    void set(Extensible* container, bool value) {
        if (value) {
            set_raw(container, this);
        } else {
            unset_raw(container);
        }
    }

    void unset(Extensible* container) {
        unset_raw(container);
    }

    void free(Extensible* container, void* item) CXX11_OVERRIDE {
        // nothing to free
    }
};

class ModuleTopicLock : public Module {
    CommandSVSTOPIC cmd;
    FlagExtItem topiclock;

  public:
    ModuleTopicLock()
        : cmd(this), topiclock("topiclock", this) {
    }

    ModResult OnPreTopicChange(User* user, Channel* chan,
                               const std::string &topic) CXX11_OVERRIDE {
        // Only fired for local users currently, but added a check anyway
        if ((IS_LOCAL(user)) && (topiclock.get(chan))) {
            user->WriteNumeric(ERR_TOPICLOCK, chan->name,
                               "TOPIC cannot be changed due to topic lock being active on the channel");
            return MOD_RES_DENY;
        }

        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows services to lock the channel topic so that it can not be changed.", VF_COMMON | VF_VENDOR);
    }
};

MODULE_INIT(ModuleTopicLock)
