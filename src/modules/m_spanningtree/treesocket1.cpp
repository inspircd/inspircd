/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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
#include "iohook.h"

#include "main.h"
#include "modules/spanningtree.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "commands.h"

/** Because most of the I/O gubbins are encapsulated within
 * BufferedSocket, we just call the superclass constructor for
 * most of the action, and append a few of our own values
 * to it.
 */
TreeSocket::TreeSocket(Link* link, Autoconnect* myac, const std::string& ipaddr)
	: linkID(assign(link->Name)), LinkState(CONNECTING), MyRoot(NULL), proto_version(0)
	, burstsent(false), age(ServerInstance->Time())
{
	capab = new CapabData;
	capab->link = link;
	capab->ac = myac;
	capab->capab_phase = 0;

	DoConnect(ipaddr, link->Port, link->Timeout, link->Bind);
	Utils->timeoutlist[this] = std::pair<std::string, int>(linkID, link->Timeout);
	SendCapabilities(1);
}

/** When a listening socket gives us a new file descriptor,
 * we must associate it with a socket without creating a new
 * connection. This constructor is used for this purpose.
 */
TreeSocket::TreeSocket(int newfd, ListenSocket* via, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
	: BufferedSocket(newfd)
	, linkID("inbound from " + client->addr()), LinkState(WAIT_AUTH_1), MyRoot(NULL), proto_version(0)
	, burstsent(false), age(ServerInstance->Time())
{
	capab = new CapabData;
	capab->capab_phase = 0;

	if (via->iohookprov)
		via->iohookprov->OnAccept(this, client, server);
	SendCapabilities(1);

	Utils->timeoutlist[this] = std::pair<std::string, int>(linkID, 30);
}

ServerState TreeSocket::GetLinkState()
{
	return this->LinkState;
}

void TreeSocket::CleanNegotiationInfo()
{
	// connect is good, reset the autoconnect block (if used)
	if (capab->ac)
		capab->ac->position = -1;
	delete capab;
	capab = NULL;
}

CullResult TreeSocket::cull()
{
	Utils->timeoutlist.erase(this);
	if (capab && capab->ac)
		Utils->Creator->ConnectServer(capab->ac, false);
	return this->BufferedSocket::cull();
}

TreeSocket::~TreeSocket()
{
	delete capab;
}

/** When an outbound connection finishes connecting, we receive
 * this event, and must send our SERVER string to the other
 * side. If the other side is happy, as outlined in the server
 * to server docs on the inspircd.org site, the other side
 * will then send back its own server string.
 */
void TreeSocket::OnConnected()
{
	if (this->LinkState == CONNECTING)
	{
		if (!capab->link->Hook.empty())
		{
			ServiceProvider* prov = ServerInstance->Modules->FindService(SERVICE_IOHOOK, capab->link->Hook);
			if (!prov)
			{
				SetError("Could not find hook '" + capab->link->Hook + "' for connection to " + linkID);
				return;
			}
			static_cast<IOHookProvider*>(prov)->OnConnect(this);
		}

		ServerInstance->SNO->WriteGlobalSno('l', "Connection to \2%s\2[%s] started.", linkID.c_str(),
			(capab->link->HiddenFromStats ? "<hidden>" : capab->link->IPAddr.c_str()));
		this->SendCapabilities(1);
	}
}

void TreeSocket::OnError(BufferedSocketError e)
{
	ServerInstance->SNO->WriteGlobalSno('l', "Connection to '\002%s\002' failed with error: %s",
		linkID.c_str(), getError().c_str());
	LinkState = DYING;
}

void TreeSocket::SendError(const std::string &errormessage)
{
	WriteLine("ERROR :"+errormessage);
	DoWrite();
	LinkState = DYING;
	SetError(errormessage);
}

/** This function forces this server to quit, removing this server
 * and any users on it (and servers and users below that, etc etc).
 * It's very slow and pretty clunky, but luckily unless your network
 * is having a REAL bad hair day, this function shouldnt be called
 * too many times a month ;-)
 */
void TreeSocket::SquitServer(std::string &from, TreeServer* Current, int& num_lost_servers, int& num_lost_users)
{
	ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "SquitServer for %s from %s", Current->GetName().c_str(), from.c_str());
	/* recursively squit the servers attached to 'Current'.
	 * We're going backwards so we don't remove users
	 * while we still need them ;)
	 */
	const TreeServer::ChildServers& children = Current->GetChildren();
	for (TreeServer::ChildServers::const_iterator i = children.begin(); i != children.end(); ++i)
	{
		TreeServer* recursive_server = *i;
		this->SquitServer(from,recursive_server, num_lost_servers, num_lost_users);
	}
	/* Now we've whacked the kids, whack self */
	num_lost_servers++;
	num_lost_users += Current->QuitUsers(from);
}

/** This is a wrapper function for SquitServer above, which
 * does some validation first and passes on the SQUIT to all
 * other remaining servers.
 */
void TreeSocket::Squit(TreeServer* Current, const std::string &reason)
{
	bool LocalSquit = false;

	if (!Current->IsRoot())
	{
		DelServerEvent(Utils->Creator, Current->GetName());

		if (Current->IsLocal())
		{
			ServerInstance->SNO->WriteGlobalSno('l', "Server \002"+Current->GetName()+"\002 split: "+reason);
			LocalSquit = true;
			if (Current->GetSocket()->Introduced())
			{
				CmdBuilder params("SQUIT");
				params.push_back(Current->GetID());
				params.push_last(reason);
				params.Broadcast();
			}
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('L', "Server \002"+Current->GetName()+"\002 split from server \002"+Current->GetParent()->GetName()+"\002 with reason: "+reason);
		}
		int num_lost_servers = 0;
		int num_lost_users = 0;
		std::string from = Current->GetParent()->GetName()+" "+Current->GetName();

		ModuleSpanningTree* st = Utils->Creator;
		st->SplitInProgress = true;
		SquitServer(from, Current, num_lost_servers, num_lost_users);
		st->SplitInProgress = false;

		ServerInstance->SNO->WriteToSnoMask(LocalSquit ? 'l' : 'L', "Netsplit complete, lost \002%d\002 user%s on \002%d\002 server%s.",
			num_lost_users, num_lost_users != 1 ? "s" : "", num_lost_servers, num_lost_servers != 1 ? "s" : "");
		Current->Tidy();
		Current->GetParent()->DelChild(Current);
		Current->cull();
		const bool ismyroot = (Current == MyRoot);
		delete Current;
		if (ismyroot)
		{
			MyRoot = NULL;
			Close();
		}
	}
}

CmdResult CommandSQuit::HandleServer(TreeServer* server, std::vector<std::string>& params)
{
	TreeServer* quitting = Utils->FindServer(params[0]);
	if (!quitting)
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Squit from unknown server");
		return CMD_FAILURE;
	}

	TreeSocket* sock = server->GetSocket();
	sock->Squit(quitting, params[1]);

	// XXX: Return CMD_FAILURE when servers SQUIT themselves (i.e. :00S SQUIT 00S :Shutting down)
	// to avoid RouteCommand() being called. RouteCommand() requires a valid command source but we
	// do not have one because the server user is deleted when its TreeServer is destructed.
	// We generate a SQUIT in TreeSocket::Squit(), with our sid as the source and send it to the
	// remaining servers.
	return ((quitting == server) ? CMD_FAILURE : CMD_SUCCESS);
}

/** This function is called when we receive data from a remote
 * server.
 */
void TreeSocket::OnDataReady()
{
	Utils->Creator->loopCall = true;
	std::string line;
	while (GetNextLine(line))
	{
		std::string::size_type rline = line.find('\r');
		if (rline != std::string::npos)
			line = line.substr(0,rline);
		if (line.find('\0') != std::string::npos)
		{
			SendError("Read null character from socket");
			break;
		}

		try
		{
			ProcessLine(line);
		}
		catch (CoreException& ex)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Error while processing: " + line);
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, ex.GetReason());
			SendError(ex.GetReason() + " - check the log file for details");
		}

		if (!getError().empty())
			break;
	}
	if (LinkState != CONNECTED && recvq.length() > 4096)
		SendError("RecvQ overrun (line too long)");
	Utils->Creator->loopCall = false;
}

bool TreeSocket::Introduced()
{
	return (capab == NULL);
}
