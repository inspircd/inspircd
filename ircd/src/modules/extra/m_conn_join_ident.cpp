/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

/// $ModAuthor: genius3000
/// $ModAuthorMail: genius3000@g3k.solutions
/// $ModConfig: <autojoinident ident="bad*" chan="#bads">
/// $ModDepends: core 3
/// $ModDesc: Autojoin users to a channel based on their ident.


#include "inspircd.h"

class ModuleConnJoinIdent : public Module {
    typedef std::vector<std::pair<std::string, std::string> > IdentChanList;
    IdentChanList chans;

  public:
    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        chans.clear();

        ConfigTagList tags = ServerInstance->Config->ConfTags("autojoinident");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;
            chans.push_back(std::make_pair(tag->getString("ident"),
                                           tag->getString("chan")));
        }
    }

    void OnPostConnect(User* user) CXX11_OVERRIDE {
        LocalUser* localuser = IS_LOCAL(user);
        if (!localuser) {
            return;
        }

        for (IdentChanList::const_iterator i = chans.begin(); i != chans.end(); ++i) {
            if (!InspIRCd::Match(localuser->ident, i->first)) {
                continue;
            }

            const std::string& channame = i->second;
            if (ServerInstance->IsChannel(channame)) {
                Channel::JoinUser(localuser, channame);
            }
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Autojoins users based on their ident");
    }
};

MODULE_INIT(ModuleConnJoinIdent)
