/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014-2016 Sadie Powell <sadie@witchery.services>
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
/// $ModConfig: <autodrop commands="CONNECT DELETE GET HEAD OPTIONS PATCH POST PUT TRACE">
/// $ModDepends: core 3
/// $ModDesc: Allows clients to be automatically dropped if they execute certain commands before registration.


#include "inspircd.h"

class ModuleAutoDrop : public Module {
  private:
    std::vector<std::string> Commands;

  public:
    void Prioritize() CXX11_OVERRIDE {
        ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_FIRST);
    }

    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        Commands.clear();

        ConfigTag* tag = ServerInstance->Config->ConfValue("autodrop");
        std::string commandList = tag->getString("commands", "CONNECT DELETE GET HEAD OPTIONS PATCH POST PUT TRACE");

        irc::spacesepstream stream(commandList);
        std::string token;
        while (stream.GetToken(token)) {
            Commands.push_back(token);
        }
    }

    ModResult OnPreCommand(std::string& command, Command::Params&, LocalUser* user,
                           bool) CXX11_OVERRIDE {
        if (user->registered == REG_ALL || std::find(Commands.begin(), Commands.end(), command) == Commands.end()) {
            return MOD_RES_PASSTHRU;
        }

        user->eh.SetError("Dropped by " MODNAME);
        return MOD_RES_DENY;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows clients to be automatically dropped if they execute certain commands before registration.");
    }
};

MODULE_INIT(ModuleAutoDrop)
