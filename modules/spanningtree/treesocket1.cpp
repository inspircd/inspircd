/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
TreeSocket::TreeSocket(const std::shared_ptr<Link>& link, const std::shared_ptr<Autoconnect>& myac, const irc::sockets::sockaddrs& dest)
	: linkID(link->Name)
	, LinkState(CONNECTING)
	, capab(std::make_unique<CapabData>(dest))
	, age(ServerInstance->Time())
{
	capab->link = link;
	capab->ac = myac;

	irc::sockets::sockaddrs bind;
	if (!link->Bind.empty() && (dest.family() == AF_INET || dest.family() == AF_INET6))
	{
		if (!bind.from_ip(link->Bind))
		{
			state = I_ERROR;
			SetError("Bind address '" + link->Bind + "' is not a valid IPv4 or IPv6 address");
			TreeSocket::OnError(I_ERR_BIND);
			return;
		}
		else if (bind.family() != dest.family())
		{
			state = I_ERROR;
			SetError("Bind address '" + bind.addr() + "' is not the same address family as destination address '" + dest.addr() + "'");
			TreeSocket::OnError(I_ERR_BIND);
			return;
		}
	}

	DoConnect(dest, bind, link->Timeout, link->Protocol);
	Utils->timeoutlist[this] = std::pair<std::string, unsigned int>(linkID, link->Timeout);
	SendCapabilities(1);
}

/** Constructor for incoming connections
 */
TreeSocket::TreeSocket(int newfd, ListenSocket* via, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server)
	: BufferedSocket(newfd)
	, linkID("inbound from " + client.addr())
	, LinkState(WAIT_AUTH_1)
	, capab(std::make_unique<CapabData>(client))
	, age(ServerInstance->Time())
{
	for (auto& iohookprovref : via->iohookprovs)
	{
		if (!iohookprovref)
			continue;

		iohookprovref->OnAccept(this, client, server);
		// IOHook could have encountered a fatal error, e.g. if the TLS ClientHello was already in the queue and there was no common TLS version
		if (!GetError().empty())
		{
			TreeSocket::OnError(I_ERR_OTHER);
			return;
		}
	}

	SendCapabilities(1);

	Utils->timeoutlist[this] = std::pair<std::string, unsigned int>(linkID, 30);
}

void TreeSocket::CleanNegotiationInfo()
{
	// connect is good, reset the autoconnect block (if used)
	if (capab->ac)
		capab->ac->position = -1;
	capab.reset();
}

Cullable::Result TreeSocket::Cull()
{
	Utils->timeoutlist.erase(this);
	if (capab && capab->ac)
		Utils->Creator->ConnectServer(capab->ac, false);
	return this->BufferedSocket::Cull();
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
			ServiceProvider* prov = ServerInstance->Modules.FindService(SERVICE_IOHOOK, "ssl/" + capab->link->Hook);
			if (!prov)
			{
				SetError("Could not find hook '" + capab->link->Hook + "' for connection to " + linkID);
				return;
			}
			static_cast<IOHookProvider*>(prov)->OnConnect(this);
		}

		ServerInstance->SNO.WriteGlobalSno('l', "Connection to \002{}\002[{}] started.", linkID,
			(capab->link->HiddenFromStats ? "<hidden>" : capab->link->IPAddr));
		this->SendCapabilities(1);
	}
}

void TreeSocket::OnError(BufferedSocketError e)
{
	ServerInstance->SNO.WriteGlobalSno('l', "Connection to '\002{}\002' failed with error: {}",
		linkID, GetError());
	LinkState = DYING;
	Close();
}

void TreeSocket::SendError(const std::string& errormessage)
{
	WriteLine("ERROR :"+errormessage);
	DoWrite();
	LinkState = DYING;
	SetError(errormessage);
}

CmdResult CommandSQuit::HandleServer(TreeServer* server, CommandBase::Params& params)
{
	TreeServer* quitting = Utils->FindServer(params[0]);
	if (!quitting)
	{
		ServerInstance->Logs.Normal(MODNAME, "Squit from unknown server");
		return CmdResult::FAILURE;
	}

	CmdResult ret = CmdResult::SUCCESS;
	if (quitting == server)
	{
		ret = CmdResult::FAILURE;
		server = server->GetTreeParent();
	}
	else if (quitting->GetTreeParent() != server)
		throw ProtocolException("Attempted to SQUIT a non-directly connected server or the parent");

	server->SQuitChild(quitting, params[1]);

	// XXX: Return CmdResult::FAILURE when servers SQUIT themselves (i.e. :00S SQUIT 00S :Shutting down)
	// to stop this message from being forwarded.
	// The squit logic generates a SQUIT message with our sid as the source and sends it to the
	// remaining servers.
	return ret;
}

bool TreeSocket::GetNextLine(std::string& line, char delim)
{
	std::string::size_type i = recvq.find(delim);
	if (i == std::string::npos)
		return false;
	line.assign(recvq, 0, i);
	recvq.erase(0, i + 1);
	return true;
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
		catch (const CoreException& ex)
		{
			ServerInstance->Logs.Normal(MODNAME, "Error while processing: " + line);
			ServerInstance->Logs.Normal(MODNAME, ex.GetReason());
			SendError(ex.GetReason() + " - check the log file for details");
		}

		if (!GetError().empty())
			break;
	}
	if (LinkState != CONNECTED && recvq.length() > 4096)
		SendError("RecvQ overrun (line too long)");
	Utils->Creator->loopCall = false;
}

static std::string newline("\n");

void TreeSocket::WriteLineInternal(const std::string& line)
{
	ServerInstance->Logs.RawIO(MODNAME, "S[{}] O {}", GetFd(), line);
	this->WriteData(line);
	this->WriteData(newline);
}
