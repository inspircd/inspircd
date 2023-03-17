/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
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
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

CmdResult CommandIJoin::HandleRemote(RemoteUser* user, Params& params) {
    Channel* chan = ServerInstance->FindChan(params[0]);
    if (!chan) {
        // Desync detected, recover
        // Ignore the join and send RESYNC, this will result in the remote server sending all channel data to us
        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG,
                                  "Received IJOIN for nonexistent channel: " + params[0]);

        CmdBuilder("RESYNC").push(params[0]).Unicast(user);

        return CMD_FAILURE;
    }

    bool apply_modes;
    if (params.size() > 3) {
        time_t RemoteTS = ServerCommand::ExtractTS(params[2]);
        apply_modes = (RemoteTS <= chan->age);
    } else {
        apply_modes = false;
    }

    // Join the user and set the membership id to what they sent
    Membership* memb = chan->ForceJoin(user, apply_modes ? &params[3] : NULL);
    if (!memb) {
        return CMD_FAILURE;
    }

    memb->id = Membership::IdFromString(params[1]);
    return CMD_SUCCESS;
}

CmdResult CommandResync::HandleServer(TreeServer* server,
                                      CommandBase::Params& params) {
    ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Resyncing " + params[0]);
    Channel* chan = ServerInstance->FindChan(params[0]);
    if (!chan) {
        // This can happen for a number of reasons, safe to ignore
        ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Channel does not exist");
        return CMD_FAILURE;
    }

    if (!server->IsLocal()) {
        throw ProtocolException("RESYNC from a server that is not directly connected");
    }

    // Send all known information about the channel
    server->GetSocket()->SyncChannel(chan);
    return CMD_SUCCESS;
}
