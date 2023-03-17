/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 bigfoot547 <bigfoot@bigfootslair.net>
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

/// $ModAuthor: bigfoot547
/// $ModAuthorMail: bigfoot@bigfootslair.net
/// $ModConfig: <timedstaticquit quitmsg="Client Quit" mintime="5m">
/// $ModDepends: core 3
/// $ModDesc: Replaces the quit message of a quitting user if they have been connected for less than a configurable time


#include "inspircd.h"

class ModuleTimedStaticQuit : public Module {
  private:
    std::string quitmsg;
    unsigned int mintime;

  public:
    ModResult OnPreCommand(std::string& command, CommandBase::Params&,
                           LocalUser* user, bool validated) CXX11_OVERRIDE {
        // We check if the user has done his due time on the network
        if (validated && (command == "QUIT")) {
            time_t now = ServerInstance->Time();
            if ((now - user->signon) < mintime) {
                ServerInstance->Users->QuitUser(user, quitmsg);
                return MOD_RES_DENY;
            }
        }
        return MOD_RES_PASSTHRU;
    }

    void ReadConfig(ConfigStatus&) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("timedstaticquit");
        this->quitmsg = tag->getString("quitmsg", "Client Quit");
        this->mintime = tag->getDuration("mintime", 5*60, 1, 60*60);
    }

    Version GetVersion() CXX11_OVERRIDE {
        // It was late, mk? Make me a better description pls :)
        return Version("Replaces the quit message of a quitting user if they have been connected for less than a configurable time", VF_OPTCOMMON);
    }

    void Prioritize() CXX11_OVERRIDE {
        // Since we take the quit command, we go last
        ServerInstance->Modules->SetPriority(this, I_OnPreCommand, PRIORITY_LAST);
    }
};

MODULE_INIT(ModuleTimedStaticQuit)
