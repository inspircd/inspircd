/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "commands.h"

CommandRSQuit::CommandRSQuit(Module* Creator)
    : Command(Creator, "RSQUIT", 1) {
    flags_needed = 'o';
    syntax = "<target-server-mask> [:<reason>]";
}

CmdResult CommandRSQuit::Handle(User* user, const Params& parameters) {
    TreeServer *server_target; // Server to squit

    server_target = Utils->FindServerMask(parameters[0]);
    if (!server_target) {
        user->WriteRemoteNotice(
            InspIRCd::Format("*** RSQUIT: Server \002%s\002 isn't connected to the network!",
                             parameters[0].c_str()));
        return CMD_FAILURE;
    }

    if (server_target->IsRoot()) {
        user->WriteRemoteNotice(
            InspIRCd::Format("*** RSQUIT: Foolish mortal, you cannot make a server SQUIT itself! (%s matches local server name)",
                             parameters[0].c_str()));
        return CMD_FAILURE;
    }

    if (server_target->IsLocal()) {
        // We have been asked to remove server_target.
        const char* reason = parameters.size() == 2 ? parameters[1].c_str() :
                             "No reason";
        ServerInstance->SNO->WriteToSnoMask('l',
                                            "RSQUIT: Server \002%s\002 removed from network by %s (%s)",
                                            parameters[0].c_str(), user->nick.c_str(), reason);
        server_target->SQuit("Server quit by " + user->GetFullRealHost() + " (" + reason
                             + ")");
    }

    return CMD_SUCCESS;
}

RouteDescriptor CommandRSQuit::GetRouting(User* user,
        const Params& parameters) {
    return ROUTE_UNICAST(parameters[0]);
}
