/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Matt Schatz <genius3000@g3k.solutions>
 *
 * This file is a module for InspIRCd.  InspIRCd is free software: you can
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
/// $ModDepends: core 3
/// $ModDesc: Provides extban 'b' - Ban list from another channel

/* Helpop Lines for the EXTBANS section
 * Find: '<helpop key="extbans" title="Extended Bans" value="'
 * Place just before the 'j:<channel>' line:
 b:<channel>   Matches users banned in the given channel
               requires the extbanbanlist contrib module).
 */


#include "inspircd.h"
#include "listmode.h"

class ExtbanBanlist : public ModeWatcher {
    ChanModeReference& banmode;

  public:
    ExtbanBanlist(Module* parent, ChanModeReference& moderef)
        : ModeWatcher(parent, "ban", MODETYPE_CHANNEL)
        , banmode(moderef) {
    }

    bool BeforeMode(User* source, User*, Channel* channel, std::string& param,
                    bool adding) CXX11_OVERRIDE {
        if (!IS_LOCAL(source) || !channel || !adding || param.length() < 3) {
            return true;
        }

        // Check for a match to both a regular and nested extban
        if ((param[0] != 'b' || param[1] != ':') && param.find(":b:") == std::string::npos) {
            return true;
        }

        std::string chan = param.substr(param.find("b:") + 2);

        Channel* c = ServerInstance->FindChan(chan);
        if (!c) {
            source->WriteNumeric(Numerics::NoSuchChannel(chan));
            return false;
        }

        if (c == channel) {
            source->WriteNumeric(ERR_NOSUCHCHANNEL, chan,
                                 "Target channel must be a different channel");
            return false;
        }

        if (banmode->GetLevelRequired(adding) > c->GetPrefixValue(source)) {
            source->WriteNumeric(ERR_CHANOPRIVSNEEDED, chan,
                                 "You must have access to modify the banlist to use it");
            return false;
        }

        return true;
    }
};

class ModuleExtbanBanlist : public Module {
    ChanModeReference banmode;
    ExtbanBanlist eb;
    bool checking;

  public:
    ModuleExtbanBanlist()
        : banmode(this, "ban")
        , eb(this, banmode)
        , checking(false) {
    }

    ModResult OnCheckBan(User* user, Channel* c,
                         const std::string& mask) CXX11_OVERRIDE {
        if (!checking && (mask.length() > 2) && (mask[0] == 'b') && (mask[1] == ':')) {
            Channel* chan = ServerInstance->FindChan(mask.substr(2));
            if (!chan) {
                return MOD_RES_PASSTHRU;
            }

            ListModeBase* banlm = banmode->IsListModeBase();
            const ListModeBase::ModeList* bans = banlm ? banlm->GetList(chan) : NULL;
            if (!bans) {
                return MOD_RES_PASSTHRU;
            }

            for (ListModeBase::ModeList::const_iterator i = bans->begin(); i != bans->end();
                    ++i) {
                checking = true;
                bool hit = chan->CheckBan(user, i->mask);
                checking = false;

                if (hit) {
                    return MOD_RES_DENY;
                }
            }
        }
        return MOD_RES_PASSTHRU;
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back('b');
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Extban 'b' - ban list from another channel", VF_OPTCOMMON);
    }
};

MODULE_INIT(ModuleExtbanBanlist)
