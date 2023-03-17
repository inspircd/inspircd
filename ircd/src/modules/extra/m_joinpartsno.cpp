/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
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

/// $ModAuthor: Shawn Smith
/// $ModDesc: Sends server notices when a user joins/parts a channel.
/// $ModDepends: core 3

class ModuleJoinPartSNO : public Module {
  public:
    void init() CXX11_OVERRIDE {
        ServerInstance->SNO->EnableSnomask('e', "JOIN");
        ServerInstance->SNO->EnableSnomask('p', "PART");
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Creates SNOMask for user joins/parts");
    }

    void OnUserJoin(Membership* memb, bool sync, bool created,
                    CUList& except) CXX11_OVERRIDE {
        /* If it's a local user do e, else E. */
        ServerInstance->SNO->WriteToSnoMask((IS_LOCAL(memb->user) ? 'e' : 'E'), "User %s joined %s", memb->user->GetFullRealHost().c_str(), memb->chan->name.c_str());
    }

    void OnUserPart(Membership* memb, std::string& partmessage,
                    CUList& except) CXX11_OVERRIDE {
        ServerInstance->SNO->WriteToSnoMask((IS_LOCAL(memb->user) ? 'p' : 'P'), "User %s parted %s", memb->user->GetFullRealHost().c_str(), memb->chan->name.c_str());
    }
};

MODULE_INIT(ModuleJoinPartSNO)
