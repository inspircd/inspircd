/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Renegade334 <contact.caaeed4f@renegade334.me.uk>
 *   Copyright (C) 2013, 2018, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

class ModuleGecosBan : public Module {
  public:
    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds extended bans a: (realmask) and r:(realname) which checks whether users have a real name (gecos) matching the specified glob pattern.", VF_OPTCOMMON|VF_VENDOR);
    }

    ModResult OnCheckBan(User *user, Channel *c,
                         const std::string& mask) CXX11_OVERRIDE {
        if ((mask.length() > 2) && (mask[1] == ':')) {
            if (mask[0] == 'r') {
                if (InspIRCd::Match(user->GetRealName(), mask.substr(2))) {
                    return MOD_RES_DENY;
                }
            } else if (mask[0] == 'a') {
                // Check that the user actually specified a real name.
                const size_t divider = mask.find('+', 1);
                if (divider == std::string::npos) {
                    return MOD_RES_PASSTHRU;
                }

                // Check whether the user's mask matches.
                if (!c->CheckBan(user, mask.substr(2, divider - 2))) {
                    return MOD_RES_PASSTHRU;
                }

                // Check whether the user's real name matches.
                if (InspIRCd::Match(user->GetRealName(), mask.substr(divider + 1))) {
                    return MOD_RES_DENY;
                }
            }
        }
        return MOD_RES_PASSTHRU;
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back('a');
        tokens["EXTBAN"].push_back('r');
    }
};

MODULE_INIT(ModuleGecosBan)
