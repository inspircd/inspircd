/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
#include "utils.h"
#include "treeserver.h"
#include "protocolinterface.h"
#include "commands.h"

/*
 * For documentation on this class, see include/protocol.h.
 */

void SpanningTreeProtocolInterface::GetServerList(ServerList& sl)
{
	for (const auto& [_, server] : Utils->serverlist)
	{
		ServerInfo ps;
		ps.servername = server->GetName();
		TreeServer* s = server->GetParent();
		ps.parentname = s ? s->GetName() : "";
		ps.usercount = server->UserCount;
		ps.opercount = server->OperCount;
		ps.description = server->GetDesc();
		ps.latencyms = server->rtt;
		sl.push_back(ps);
	}
}

bool SpanningTreeProtocolInterface::SendEncapsulatedData(const std::string& targetmask, const std::string& cmd, const CommandBase::Params& params, const User* source)
{
	if (!source)
		source = ServerInstance->FakeClient;

	CmdBuilder encap(source, "ENCAP");

	// Are there any wildcards in the target string?
	if (targetmask.find_first_of("*?") != std::string::npos)
	{
		// Yes, send the target string as-is; servers will decide whether or not it matches them
		encap.push(targetmask).push(cmd).insert(params).Broadcast();
	}
	else
	{
		// No wildcards which means the target string has to be the name of a known server
		TreeServer* server = Utils->FindServer(targetmask);
		if (!server)
			return false;

		// Use the SID of the target in the message instead of the server name
		encap.push(server->GetId()).push(cmd).insert(params).Unicast(server->ServerUser);
	}

	return true;
}

void SpanningTreeProtocolInterface::BroadcastEncap(const std::string& cmd, const CommandBase::Params& params, const User* source, const User* omit)
{
	if (!source)
		source = ServerInstance->FakeClient;

	// If omit is non-NULL we pass the route belonging to the user to Forward(),
	// otherwise we pass NULL, which is equivalent to Broadcast()
	TreeServer* server = (omit ? TreeServer::Get(omit)->GetRoute() : nullptr);
	CmdBuilder(source, "ENCAP * ").push_raw(cmd).insert(params).Forward(server);
}

void SpanningTreeProtocolInterface::SendMetadata(const Extensible* ext, const std::string& key, const std::string& data)
{
	CommandMetadata::Builder(ext, key, data).Broadcast();
}

void SpanningTreeProtocolInterface::SendMetadata(const std::string& key, const std::string& data)
{
	CommandMetadata::Builder(key, data).Broadcast();
}

void SpanningTreeProtocolInterface::Server::SendMetadata(const std::string& key, const std::string& data)
{
	sock->WriteLine(CommandMetadata::Builder(key, data));
}

void SpanningTreeProtocolInterface::SendSNONotice(char snomask, const std::string& text)
{
	CmdBuilder("SNONOTICE").push(snomask).push_last(text).Broadcast();
}

void SpanningTreeProtocolInterface::SendMessage(const Channel* target, char status, const std::string& text, MessageType msgtype)
{
	const char* cmd = (msgtype == MessageType::PRIVMSG ? "PRIVMSG" : "NOTICE");
	CUList exempt_list;
	ClientProtocol::TagMap tags;
	Utils->SendChannelMessage(ServerInstance->FakeClient, target, text, status, tags, exempt_list, cmd);
}

void SpanningTreeProtocolInterface::SendMessage(const User* target, const std::string& text, MessageType msgtype)
{
	CmdBuilder p(msgtype == MessageType::PRIVMSG ? "PRIVMSG" : "NOTICE");
	p.push(target->uuid);
	p.push_last(text);
	p.Unicast(target);
}
