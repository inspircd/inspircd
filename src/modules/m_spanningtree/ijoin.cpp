/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
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

CmdResult CommandIJoin::HandleRemote(const std::vector<std::string>& params, RemoteUser* user)
{
	Channel* chan = ServerInstance->FindChan(params[0]);
	if (!chan)
	{
		// Desync detected, recover
		// Ignore the join and send RESYNC, this will result in the remote server sending all channel data to us
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Received IJOIN for non-existant channel: " + params[0]);

		parameterlist p;
		p.push_back(params[0]);
		SpanningTreeUtilities* Utils = ((ModuleSpanningTree*)(Module*)creator)->Utils;
		Utils->DoOneToOne(ServerInstance->Config->GetSID(), "RESYNC", p, user->server);

		return CMD_FAILURE;
	}

	bool apply_modes;
	if (params.size() > 1)
	{
		time_t RemoteTS = ConvToInt(params[1]);
		if (!RemoteTS)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Invalid TS in IJOIN: " + params[1]);
			return CMD_INVALID;
		}

		if (RemoteTS < chan->age)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Attempted to lower TS via IJOIN. Channel=" + params[0] + " RemoteTS=" + params[1] + " LocalTS=" + ConvToStr(chan->age));
			return CMD_INVALID;
		}
		apply_modes = ((params.size() > 2) && (RemoteTS == chan->age));
	}
	else
		apply_modes = false;

	chan->ForceJoin(user, apply_modes ? &params[2] : NULL);
	return CMD_SUCCESS;
}

CmdResult CommandResync::HandleServer(const std::vector<std::string>& params, FakeUser* user)
{
	ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Resyncing " + params[0]);
	Channel* chan = ServerInstance->FindChan(params[0]);
	if (!chan)
	{
		// This can happen for a number of reasons, safe to ignore
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Channel does not exist");
		return CMD_FAILURE;
	}

	SpanningTreeUtilities* Utils = ((ModuleSpanningTree*)(Module*)creator)->Utils;
	TreeServer* server = Utils->FindServer(user->server);
	if (!server)
		return CMD_FAILURE;

	TreeSocket* socket = server->GetSocket();
	if (!socket)
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Received RESYNC with a source that is not directly connected: " + user->uuid);
		return CMD_INVALID;
	}

	// Send all known information about the channel
	socket->SyncChannel(chan);
	return CMD_SUCCESS;
}
