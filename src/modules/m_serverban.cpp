/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
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

class ModuleServerBan : public Module {
  public:
    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds extended ban s: (server) which check whether users are on a server matching the specified glob pattern.", VF_OPTCOMMON|VF_VENDOR);
    }

    ModResult OnCheckBan(User *user, Channel *c,
                         const std::string& mask) CXX11_OVERRIDE {
        if ((mask.length() > 2) && (mask[0] == 's') && (mask[1] == ':')) {
            if (InspIRCd::Match(user->server->GetPublicName(), mask.substr(2))) {
                return MOD_RES_DENY;
            }
        }
        return MOD_RES_PASSTHRU;
    }

    void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE {
        tokens["EXTBAN"].push_back('s');
    }
};

MODULE_INIT(ModuleServerBan)
