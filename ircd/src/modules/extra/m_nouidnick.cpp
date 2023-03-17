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

/// $ModAuthor: Attila Molnar
/// $ModAuthorMail: attilamolnar@hush.com
/// $ModDepends: core 3
/// $ModDesc: Disallows changing nick to UID using /NICK


#include "inspircd.h"

class ModuleNoUIDNicks : public Module {
  public:
    ModResult OnUserPreNick(LocalUser* user,
                            const std::string& newnick) CXX11_OVERRIDE {
        if ((newnick[0] > '9') || (newnick[0] < '0')) {
            return MOD_RES_PASSTHRU;
        }

        user->WriteNumeric(ERR_ERRONEUSNICKNAME, 0, "Erroneous Nickname");
        return MOD_RES_DENY;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Disallows changing nick to UID using /NICK");
    }
};

MODULE_INIT(ModuleNoUIDNicks)
