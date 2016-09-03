/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
	for (server_hash::iterator i = Utils->serverlist.begin(); i != Utils->serverlist.end(); i++)
	{
		ServerInfo ps;
		ps.servername = i->second->GetName();
		TreeServer* s = i->second->GetParent();
		ps.parentname = s ? s->GetName() : "";
		ps.usercount = i->second->UserCount;
		ps.opercount = i->second->OperCount;
		ps.gecos = i->second->GetDesc();
		ps.latencyms = i->second->rtt;
		sl.push_back(ps);
	}
}

bool SpanningTreeProtocolInterface::SendEncapsulatedData(const std::string& targetmask, const std::string& cmd, const parameterlist& params, User* source)
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
		encap.push(server->GetID()).push(cmd).insert(params).Unicast(server->ServerUser);
	}

	return true;
}

void SpanningTreeProtocolInterface::BroadcastEncap(const std::string& cmd, const parameterlist& params, User* source, User* omit)
{
	if (!source)
		source = ServerInstance->FakeClient;

	// If omit is non-NULL we pass the route belonging to the user to Forward(),
	// otherwise we pass NULL, which is equivalent to Broadcast()
	TreeServer* server = (omit ? TreeServer::Get(omit)->GetRoute() : NULL);
	CmdBuilder(source, "ENCAP * ").push_raw(cmd).insert(params).Forward(server);
}

void SpanningTreeProtocolInterface::SendMetaData(User* u, const std::string& key, const std::string& data)
{
	CommandMetadata::Builder(u, key, data).Broadcast();
}

void SpanningTreeProtocolInterface::SendMetaData(Channel* c, const std::string& key, const std::string& data)
{
	CommandMetadata::Builder(c, key, data).Broadcast();
}

void SpanningTreeProtocolInterface::SendMetaData(const std::string& key, const std::string& data)
{
	CommandMetadata::Builder(key, data).Broadcast();
}

void SpanningTreeProtocolInterface::Server::SendMetaData(const std::string& key, const std::string& data)
{
	sock->WriteLine(CommandMetadata::Builder(key, data));
}

void SpanningTreeProtocolInterface::SendSNONotice(char snomask, const std::string &text)
{
	CmdBuilder("SNONOTICE").push(snomask).push_last(text).Broadcast();
}

void SpanningTreeProtocolInterface::SendMessage(Channel* target, char status, const std::string& text, MessageType msgtype)
{
	const char* cmd = (msgtype == MSG_PRIVMSG ? "PRIVMSG" : "NOTICE");
	CUList exempt_list;
	Utils->SendChannelMessage(ServerInstance->Config->GetSID(), target, text, status, exempt_list, cmd);
}

void SpanningTreeProtocolInterface::SendMessage(User* target, const std::string& text, MessageType msgtype)
{
	CmdBuilder p(msgtype == MSG_PRIVMSG ? "PRIVMSG" : "NOTICE");
	p.push_back(target->uuid);
	p.push_last(text);
	p.Unicast(target);
}
