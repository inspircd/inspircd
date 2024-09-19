/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Dominic Hamon
 *   Copyright (C) 2018-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
#include "xline.h"
#include "main.h"

#include "utils.h"
#include "treeserver.h"

/** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
 * represents our own server. Therefore, it has no route, no parent, and
 * no socket associated with it. Its version string is our own local version.
 */
TreeServer::TreeServer()
	: Server(ServerInstance->Config->ServerId, ServerInstance->Config->ServerName, ServerInstance->Config->ServerDesc)
	, pingtimer(this)
	, ServerUser(ServerInstance->FakeClient)
	, age(ServerInstance->Time())
	, UserCount(ServerInstance->Users.LocalUserCount())
	, customversion(ServerInstance->Config->CustomVersion)
	, rawbranch(INSPIRCD_BRANCH)
	, rawversion(INSPIRCD_VERSION)
{
	AddHashEntry();
}

/** When we create a new server, we call this constructor to initialize it.
 * This constructor initializes the server's Route and Parent, and sets up
 * the ping timer for the server.
 */
TreeServer::TreeServer(const std::string& Name, const std::string& Desc, const std::string& Sid, TreeServer* Above, TreeSocket* Sock, bool Hide)
	: Server(Sid, Name, Desc)
	, Parent(Above)
	, Socket(Sock)
	, behind_bursting(Parent->behind_bursting)
	, pingtimer(this)
	, ServerUser(new FakeUser(id, this))
	, age(ServerInstance->Time())
	, rawbranch("unknown")
	, rawversion("unknown")
	, Hidden(Hide)
{
	ServerInstance->Logs.Debug(MODNAME, "New server {} behind_bursting {}", GetName(), behind_bursting);
	CheckService();

	ServerInstance->Timers.AddTimer(&pingtimer);

	/* find the 'route' for this server (e.g. the one directly connected
	 * to the local server, which we can use to reach it)
	 *
	 * In the following example, consider we have just added a TreeServer
	 * class for server G on our network, of which we are server A.
	 * To route traffic to G (marked with a *) we must send the data to
	 * B (marked with a +) so this algorithm initializes the 'Route'
	 * value to point at whichever server traffic must be routed through
	 * to get here. If we were to try this algorithm with server B,
	 * the Route pointer would point at its own object ('this').
	 *
	 *            A
	 *           / \
	 *        + B   C
	 *         / \   \
	 *        D   E   F
	 *       /         \
	 *    * G           H
	 *
	 * We only run this algorithm when a server is created, as
	 * the routes remain constant while ever the server exists, and
	 * do not need to be re-calculated.
	 */

	Route = Above;
	if (Route == Utils->TreeRoot)
	{
		Route = this;
	}
	else
	{
		while (this->Route->GetTreeParent() != Utils->TreeRoot)
		{
			this->Route = Route->GetTreeParent();
		}
	}

	/* Because recursive code is slow and takes a lot of resources,
	 * we store two representations of the server tree. The first
	 * is a recursive structure where each server references its
	 * children and its parent, which is used for netbursts and
	 * netsplits to dump the whole dataset to the other server,
	 * and the second is used for very fast lookups when routing
	 * messages and is instead a hash_map, where each item can
	 * be referenced by its server name. The AddHashEntry()
	 * call below automatically inserts each TreeServer class
	 * into the hash_map as it is created. There is a similar
	 * maintenance call in the destructor to tidy up deleted
	 * servers.
	 */

	this->AddHashEntry();
	Parent->Children.push_back(this);

	Utils->Creator->linkeventprov.Call(&ServerProtocol::LinkEventListener::OnServerLink, this);
}

void TreeServer::BeginBurst(uint64_t startms)
{
	behind_bursting++;

	uint64_t now = ServerInstance->Time() * 1000 + (ServerInstance->Time_ns() / 1000000);
	// If the start time is in the future (clocks are not synced) then use current time
	if ((!startms) || (startms > now))
		startms = now;
	this->StartBurst = startms;
	ServerInstance->Logs.Debug(MODNAME, "Server {} started bursting at time {} behind_bursting {}", GetId(), startms, behind_bursting);
}

void TreeServer::FinishBurstInternal()
{
	// Check is needed because some older servers don't send the bursting state of a server, so servers
	// introduced during a netburst may later send ENDBURST which would normally decrease this counter
	if (behind_bursting > 0)
		behind_bursting--;
	ServerInstance->Logs.Debug(MODNAME, "FinishBurstInternal() {} behind_bursting {}", GetName(), behind_bursting);

	for (auto* child : Children)
		child->FinishBurstInternal();
}

void TreeServer::FinishBurst()
{
	ServerInstance->XLines->ApplyLines();
	uint64_t ts = ServerInstance->Time() * 1000 + (ServerInstance->Time_ns() / 1000000);
	unsigned long bursttime = ts - this->StartBurst;
	ServerInstance->SNO.WriteToSnoMask(Parent == Utils->TreeRoot ? 'l' : 'L', "Received end of netburst from \002{}\002 (burst time: {} {})",
		GetName(), (bursttime > 10000 ? bursttime / 1000 : bursttime), (bursttime > 10000 ? "secs" : "msecs"));
	Utils->Creator->linkeventprov.Call(&ServerProtocol::LinkEventListener::OnServerBurst, this);

	StartBurst = 0;
	FinishBurstInternal();
}

void TreeServer::SQuitChild(TreeServer* server, const std::string& reason, bool error)
{
	std::erase(Children, server);

	if (IsRoot())
	{
		// Server split from us, generate a SQUIT message and broadcast it
		ServerInstance->SNO.WriteGlobalSno('l', "Server \002" + server->GetName() + "\002 split: " + reason);
		CmdBuilder("SQUIT").push(server->GetId()).push_last(reason).Broadcast();
	}
	else
	{
		ServerInstance->SNO.WriteToSnoMask('L', "Server \002" + server->GetName() + "\002 split from server \002" + GetName() + "\002 with reason: " + reason);
	}

	unsigned int num_lost_servers = 0;
	server->SQuitInternal(num_lost_servers, error);

	const std::string quitreason = GetName() + " " + server->GetName();
	size_t num_lost_users = QuitUsers(quitreason);

	ServerInstance->SNO.WriteToSnoMask(IsRoot() ? 'l' : 'L', "Netsplit complete, lost \002{}\002 user{} on \002{}\002 server{}.",
		num_lost_users, num_lost_users != 1 ? "s" : "", num_lost_servers, num_lost_servers != 1 ? "s" : "");

	// No-op if the socket is already closed (i.e. it called us)
	if (server->IsLocal())
		server->GetSocket()->Close();

	// Add the server to the cull list, the servers behind it are handled by Cull() and the destructor
	ServerInstance->GlobalCulls.AddItem(server);
}

void TreeServer::SQuitInternal(unsigned int& num_lost_servers, bool error)
{
	// Don't squit a server which is already dead.
	if (isdead)
		return;

	ServerInstance->Logs.Debug(MODNAME, "Server {} lost in split", GetName());

	for (auto* server : Children)
		server->SQuitInternal(num_lost_servers, error);

	// Mark server as dead
	isdead = true;
	num_lost_servers++;
	RemoveHash();

	if (!Utils->Creator->dying)
		Utils->Creator->linkeventprov.Call(&ServerProtocol::LinkEventListener::OnServerSplit, this, error);
}

size_t TreeServer::QuitUsers(const std::string& reason)
{
	std::string publicreason = Utils->HideSplits ? "*.net *.split" : reason;

	const UserMap& users = ServerInstance->Users.GetUsers();
	size_t original_size = users.size();
	for (UserMap::const_iterator i = users.begin(); i != users.end(); )
	{
		User* user = i->second;
		// Increment the iterator now because QuitUser() removes the user from the container
		++i;
		TreeServer* server = TreeServer::Get(user);
		if (server->IsDead())
			ServerInstance->Users.QuitUser(user, publicreason, &reason);
	}
	return original_size - users.size();
}

void TreeServer::CheckService()
{
	service = silentservice = false;

	for (const auto& [_, tag] : ServerInstance->Config->ConfTags("services", ServerInstance->Config->ConfTags("uline")))
	{
		std::string server = tag->getString("server");
		if (irc::equals(server, GetName()))
		{
			if (this->IsRoot())
			{
				ServerInstance->Logs.Warning(MODNAME, "Servers should not mark themselves as a service (at " + tag->source.str() + ")");
				return;
			}

			service = true;
			silentservice = tag->getBool("silent");
			break;
		}
	}
}

/** This method is used to add the server to the
 * maps for linear searches. It is only called
 * by the constructors.
 */
void TreeServer::AddHashEntry()
{
	Utils->serverlist[GetName()] = this;
	Utils->sidlist[GetId()] = this;
}

Cullable::Result TreeServer::Cull()
{
	// Recursively cull all servers that are under us in the tree
	for (auto* server : Children)
		server->Cull();

	if (!IsRoot())
		ServerUser->Cull();
	return Cullable::Cull();
}

TreeServer::~TreeServer()
{
	// Recursively delete all servers that are under us in the tree first
	for (const auto* child : Children)
		delete child;

	// Delete server user unless it's us
	if (!IsRoot())
		delete ServerUser;
}

void TreeServer::RemoveHash()
{
	Utils->sidlist.erase(GetId());
	Utils->serverlist.erase(GetName());
}

void TreeServer::SendMetadata(const std::string& key, const std::string& data) const
{
	if (GetRoute() && GetRoute()->GetSocket())
		GetRoute()->GetSocket()->WriteLine(CommandMetadata::Builder(key, data));
}

void TreeServer::SendMetadata(const Extensible* ext, const std::string& key, const std::string& data) const
{
	if (GetRoute() && GetRoute()->GetSocket())
		GetRoute()->GetSocket()->WriteLine(CommandMetadata::Builder(ext, key, data));
}
