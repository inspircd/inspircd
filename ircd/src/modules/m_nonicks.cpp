/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004 Craig Edwards <brain@inspircd.org>
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
#include "modules/exemption.h"

class ModuleNoNickChange : public Module {
    CheckExemption::EventProvider exemptionprov;
    SimpleChannelModeHandler nn;
  public:
    ModuleNoNickChange()
        : exemptionprov(this)
        , nn(this, "nonick", 'N') {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds channel mode N (nonick) which prevents users from changing their nickname whilst in the channel.", VF_VENDOR);
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back('N');
    }

    ModResult OnUserPreNick(LocalUser* user,
                            const std::string& newnick) CXX11_OVERRIDE {
        for (User::ChanList::iterator i = user->chans.begin(); i != user->chans.end(); i++) {
            Channel* curr = (*i)->chan;

            ModResult res = CheckExemption::Call(exemptionprov, user, curr, "nonick");
            if (res == MOD_RES_ALLOW) {
                continue;
            }

            if (user->HasPrivPermission("channels/ignore-nonicks")) {
                continue;
            }

            bool modeset = curr->IsModeSet(nn);
            if (!curr->GetExtBanStatus(user, 'N').check(!modeset)) {
                user->WriteNumeric(ERR_CANTCHANGENICK,
                                   InspIRCd::Format("Can't change nickname while on %s (%s)",
                                                    curr->name.c_str(), modeset ? "+N is set" : "you're extbanned"));
                return MOD_RES_DENY;
            }
        }

        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleNoNickChange)
