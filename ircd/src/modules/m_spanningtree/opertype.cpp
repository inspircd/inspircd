/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "commands.h"
#include "treeserver.h"
#include "utils.h"

/** Because the core won't let users or even SERVERS set +o,
 * we use the OPERTYPE command to do this.
 */
CmdResult CommandOpertype::HandleRemote(RemoteUser* u,
                                        CommandBase::Params& params) {
    const std::string& opertype = params[0];
    if (!u->IsOper()) {
        ServerInstance->Users->all_opers.push_back(u);
    }

    ModeHandler* opermh = ServerInstance->Modes->FindMode('o', MODETYPE_USER);
    if (opermh) {
        u->SetMode(opermh, true);
    }

    ServerConfig::OperIndex::const_iterator iter =
        ServerInstance->Config->OperTypes.find(opertype);
    if (iter != ServerInstance->Config->OperTypes.end()) {
        u->oper = iter->second;
    } else {
        u->oper = new OperInfo(opertype);
    }

    if (Utils->quiet_bursts) {
        /*
         * If quiet bursts are enabled, and server is bursting or silent uline (i.e. services),
         * then do nothing. -- w00t
         */
        TreeServer* remoteserver = TreeServer::Get(u);
        if (remoteserver->IsBehindBursting() || remoteserver->IsSilentULine()) {
            return CMD_SUCCESS;
        }
    }

    ServerInstance->SNO->WriteToSnoMask('O',
                                        "From %s: User %s (%s@%s) is now a server operator of type %s",
                                        u->server->GetName().c_str(), u->nick.c_str(),u->ident.c_str(),
                                        u->GetRealHost().c_str(), opertype.c_str());
    return CMD_SUCCESS;
}

CommandOpertype::Builder::Builder(User* user)
    : CmdBuilder(user, "OPERTYPE") {
    push_last(user->oper->name);
}
