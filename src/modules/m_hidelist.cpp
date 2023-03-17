/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2017-2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

class ListWatcher : public ModeWatcher {
    // Minimum rank required to view the list
    const unsigned int minrank;

  public:
    ListWatcher(Module* mod, const std::string& modename, unsigned int rank)
        : ModeWatcher(mod, modename, MODETYPE_CHANNEL)
        , minrank(rank) {
    }

    bool BeforeMode(User* user, User* destuser, Channel* chan, std::string& param,
                    bool adding) CXX11_OVERRIDE {
        // Only handle listmode list requests
        if (!param.empty()) {
            return true;
        }

        // If the user requesting the list is a member of the channel see if they have the
        // rank required to view the list
        Membership* memb = chan->GetUser(user);
        if ((memb) && (memb->getRank() >= minrank)) {
            return true;
        }

        if (user->HasPrivPermission("channels/auspex")) {
            return true;
        }

        user->WriteNumeric(Numerics::ChannelPrivilegesNeeded(chan, minrank, InspIRCd::Format("view the channel %s list", GetModeName().c_str())));
        return false;
    }
};

class ModuleHideList : public Module {
    std::vector<ListWatcher*> watchers;

  public:
    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTagList tags = ServerInstance->Config->ConfTags("hidelist");
        typedef std::vector<std::pair<std::string, unsigned int> > NewConfigs;
        NewConfigs newconfigs;
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;
            std::string modename = tag->getString("mode");
            if (modename.empty()) {
                throw ModuleException("Empty <hidelist:mode> at " + tag->getTagLocation());
            }
            // If rank is set to 0 everyone inside the channel can view the list,
            // but non-members may not
            unsigned int rank = tag->getUInt("rank", HALFOP_VALUE);
            newconfigs.push_back(std::make_pair(modename, rank));
        }

        stdalgo::delete_all(watchers);
        watchers.clear();

        for (NewConfigs::const_iterator i = newconfigs.begin(); i != newconfigs.end(); ++i) {
            watchers.push_back(new ListWatcher(this, i->first, i->second));
        }
    }

    ~ModuleHideList() {
        stdalgo::delete_all(watchers);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows list mode lists to be hidden from users without a prefix mode ranked equal to or higher than a defined level.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleHideList)
