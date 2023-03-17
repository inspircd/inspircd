/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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
/// $ModDepends: core 3
/// $ModDesc: Adds support for Discord-style #1234 nick tags.


#include "inspircd.h"

namespace {
TR1NS::function<bool(const std::string&)> origisnick;

bool IsDiscordNick(const std::string& nick) {
    if (nick.empty() || nick.length() > ServerInstance->Config->Limits.NickMax) {
        return false;
    }

    size_t hashpos = nick.find('#');
    if (hashpos == std::string::npos) {
        return false;
    }

    for (size_t pos = hashpos + 1; pos < nick.length(); ++pos)
        if (!isdigit(nick[pos])) {
            return false;
        }

    return origisnick(nick.substr(0, hashpos - 1));
}
}

class ModuleDiscordNick : public Module {
  private:
    LocalIntExt ext;

  public:
    ModuleDiscordNick()
        : ext("nicktag", ExtensionItem::EXT_USER, this) {
        origisnick = ServerInstance->IsNick;
        ServerInstance->IsNick = &IsDiscordNick;
    }

    ~ModuleDiscordNick() {
        ServerInstance->IsNick = origisnick;
    }

    ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        if (validated && command == "NICK" && parameters[0] != "0") {
            parameters[0].append(InspIRCd::Format("#%04ld", ext.get(user)));
        }
        return MOD_RES_PASSTHRU;
    }

    void OnUserPostInit(LocalUser* user) CXX11_OVERRIDE {
        ext.set(user, ServerInstance->GenRandomInt(9999));
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Adds support for Discord-style #1234 nick tags.", VF_COMMON);
    }
};

MODULE_INIT(ModuleDiscordNick)
