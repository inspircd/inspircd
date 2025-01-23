/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020, 2022-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
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

CmdResult CommandIJoin::HandleRemote(RemoteUser* user, Params& params)
{
	auto* chan = ServerInstance->Channels.Find(params[0]);
	if (!chan)
	{
		// Desync detected, recover
		// Ignore the join and send RESYNC, this will result in the remote server sending all channel data to us
		ServerInstance->Logs.Debug(MODNAME, "Received IJOIN for nonexistent channel: {}", params[0]);

		CmdBuilder("RESYNC").push(params[0]).Unicast(user);

		return CmdResult::FAILURE;
	}

	bool apply_modes;
	if (params.size() > 3)
	{
		time_t RemoteTS = ServerCommand::ExtractTS(params[2]);
		apply_modes = (RemoteTS <= chan->age);
	}
	else
		apply_modes = false;

	// Join the user and set the membership id to what they sent
	Membership* memb = chan->ForceJoin(user, apply_modes ? &params[3] : nullptr);
	if (!memb)
		return CmdResult::FAILURE;

	memb->id = Membership::IdFromString(params[1]);
	return CmdResult::SUCCESS;
}

CmdResult CommandResync::HandleServer(TreeServer* server, CommandBase::Params& params)
{
	ServerInstance->Logs.Debug(MODNAME, "Resyncing {}", params[0]);
	auto* chan = ServerInstance->Channels.Find(params[0]);
	if (!chan)
	{
		// This can happen for a number of reasons, safe to ignore
		ServerInstance->Logs.Debug(MODNAME, "Channel does not exist");
		return CmdResult::FAILURE;
	}

	if (!server->IsLocal())
		throw ProtocolException("RESYNC from a server that is not directly connected");

	// Send all known information about the channel
	server->GetSocket()->SyncChannel(chan, server);
	return CmdResult::SUCCESS;
}
