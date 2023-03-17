/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Carsten Valdemar Munk <carsten.munk+inspircd@gmail.com>
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

/// $ModAuthor: InspIRCd Developers
/// $ModAuthorMail: noreply@inspircd.org
/// $ModDepends: core 3
/// $ModDesc: Provides the ability to close unregistered connections.


#include "inspircd.h"

class CommandClose : public Command {
  public:
    CommandClose(Module* Creator)
        : Command(Creator,"CLOSE") {
        flags_needed = 'o';
    }

    CmdResult Handle(User* src, const Params& parameters) CXX11_OVERRIDE {
        std::map<std::string,int> closed;

        const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
        for (UserManager::LocalList::const_iterator u = list.begin(); u != list.end(); ) {
            // Quitting the user removes it from the list
            LocalUser* user = *u;
            ++u;
            if (user->registered != REG_ALL) {
                ServerInstance->Users->QuitUser(user,
                                                "Closing all unknown connections per request");
                std::string key = ConvToStr(user->GetIPString())+"."+ConvToStr(
                    user->server_sa.port());
                closed[key]++;
            }
        }

        int total = 0;
        for (std::map<std::string,int>::iterator ci = closed.begin(); ci != closed.end(); ci++) {
            src->WriteNotice("*** Closed " + ConvToStr(ci->second) + " unknown " +
                             (ci->second == 1 ? "connection" : "connections") +
                             " from [" + ci->first + "]");
            total += ci->second;
        }
        if (total) {
            src->WriteNotice("*** " + ConvToStr(total) + " unknown " +
                             (total == 1 ? "connection" : "connections") + " closed");
        } else {
            src->WriteNotice("*** No unknown connections found");
        }

        return CMD_SUCCESS;
    }
};

class ModuleClose : public Module {
    CommandClose cmd;
  public:
    ModuleClose()
        : cmd(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Provides the ability to close unregistered connections.", VF_NONE);
    }
};

MODULE_INIT(ModuleClose)
