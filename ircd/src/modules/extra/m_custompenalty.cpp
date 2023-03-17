/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2020 Sadie Powell <sadie@witchery.services>
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
/// $ModConfig: <penalty name="INVITE" value="60">
/// $ModDepends: core 3
/// $ModDesc: Allows the customisation of penalty levels.


#include "inspircd.h"

class ModuleCustomPenalty : public Module {
  private:
    void SetPenalties() {
        ConfigTagList tags = ServerInstance->Config->ConfTags("penalty");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            ConfigTag* tag = i->second;

            std::string name = tag->getString("name");
            unsigned int penalty = tag->getUInt("value", 1, 1);

            Command* command = ServerInstance->Parser.GetHandler(name);
            if (!command) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "Warning: unable to find command: " + name);
                continue;
            }

            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                      "Setting the penalty for %s to %d", name.c_str(), penalty);
            command->Penalty = penalty;
        }
    }

  public:
    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        SetPenalties();
    }

    void OnLoadModule(Module*) CXX11_OVERRIDE {
        SetPenalties();
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the customisation of penalty levels.");
    }
};

MODULE_INIT(ModuleCustomPenalty)
