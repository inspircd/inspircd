/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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

class ModuleMapHide : public Module {
    std::string url;
  public:
    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        url = ServerInstance->Config->ConfValue("security")->getString("maphide");
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        if (validated && !user->IsOper() && !url.empty() && (command == "MAP" || command == "LINKS")) {
            user->WriteNotice("/" + command + " has been disabled; visit " + url);
            return MOD_RES_DENY;
        } else {
            return MOD_RES_PASSTHRU;
        }
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to replace the output of a /MAP and /LINKS with an URL.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleMapHide)
