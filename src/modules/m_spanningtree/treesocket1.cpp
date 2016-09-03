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

/** Constructor for outgoing connections.
 * Because most of the I/O gubbins are encapsulated within
 * BufferedSocket, we just call DoConnect() for most of the action,
 * and only do minor initialization tasks ourselves.
 */
TreeSocket::TreeSocket(Link* link, Autoconnect* myac, const std::string& ipaddr)
	: linkID(link->Name), LinkState(CONNECTING), MyRoot(NULL), proto_version(0)
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

/** Constructor for incoming connections
 */
TreeSocket::TreeSocket(int newfd, ListenSocket* via, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
	: BufferedSocket(newfd)
	, linkID("inbound from " + client->addr()), LinkState(WAIT_AUTH_1), MyRoot(NULL), proto_version(0)
	, burstsent(false), age(ServerInstance->Time())
{
	capab = new CapabData;
	capab->capab_phase = 0;

	for (ListenSocket::IOHookProvList::iterator i = via->iohookprovs.begin(); i != via->iohookprovs.end(); ++i)
	{
		ListenSocket::IOHookProvRef& iohookprovref = *i;
		if (!iohookprovref)
			continue;

		iohookprovref->OnAccept(this, client, server);
		// IOHook could have encountered a fatal error, e.g. if the TLS ClientHello was already in the queue and there was no common TLS version
		if (!getError().empty())
		{
			TreeSocket::OnError(I_ERR_OTHER);
			return;
		}
	}

	SendCapabilities(1);

	Utils->timeoutlist[this] = std::pair<std::string, int>(linkID, 30);
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
 * this event, and must do CAPAB negotiation with the other
 * side. If the other side is happy, as outlined in the server
 * to server docs on the inspircd.org site, the other side
 * will then send back its own SERVER string eventually.
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
	Close();
}

void TreeSocket::SendError(const std::string &errormessage)
{
	WriteLine("ERROR :"+errormessage);
	DoWrite();
	LinkState = DYING;
	SetError(errormessage);
}

CmdResult CommandSQuit::HandleServer(TreeServer* server, std::vector<std::string>& params)
{
	TreeServer* quitting = Utils->FindServer(params[0]);
	if (!quitting)
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Squit from unknown server");
		return CMD_FAILURE;
	}

	CmdResult ret = CMD_SUCCESS;
	if (quitting == server)
	{
		ret = CMD_FAILURE;
		server = server->GetParent();
	}
	else if (quitting->GetParent() != server)
		throw ProtocolException("Attempted to SQUIT a non-directly connected server or the parent");

	server->SQuitChild(quitting, params[1]);

	// XXX: Return CMD_FAILURE when servers SQUIT themselves (i.e. :00S SQUIT 00S :Shutting down)
	// to stop this message from being forwarded.
	// The squit logic generates a SQUIT message with our sid as the source and sends it to the
	// remaining servers.
	return ret;
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
			line.erase(rline);
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
