/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2020, 2022-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2008-2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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


#include <filesystem>

#include "inspircd.h"
#include "modules/ssl.h"

#include "main.h"
#include "utils.h"
#include "link.h"
#include "treeserver.h"
#include "treesocket.h"
#include "commands.h"

namespace
{
	bool RunningInContainer()
	{
		std::error_code ec;
		if (std::filesystem::is_regular_file("/.dockerenv", ec))
		{
			// We are running inside of Docker so all IP addresses are
			// non-local and as far as I can see there isn't a way to
			// reliably detect the Docker network.
			return true;
		}
		return false;
	}
}

/*
 * Some server somewhere in the network introducing another server.
 *	-- w
 */
CmdResult CommandServer::HandleServer(TreeServer* ParentOfThis, Params& params)
{
	const std::string& servername = params[0];
	const std::string& sid = params[1];
	const std::string& description = params.back();
	TreeSocket* socket = ParentOfThis->GetSocket();

	if (!InspIRCd::IsSID(sid))
	{
		socket->SendError("Invalid format server ID: "+sid+"!");
		return CmdResult::FAILURE;
	}
	TreeServer* CheckDupe = Utils->FindServer(servername);
	if (CheckDupe)
	{
		socket->SendError("Server "+servername+" already exists!");
		ServerInstance->SNO.WriteToSnoMask('L', "Server \002"+CheckDupe->GetName()+"\002 being introduced from \002" + ParentOfThis->GetName() + "\002 denied, already exists. Closing link with " + ParentOfThis->GetName());
		return CmdResult::FAILURE;
	}
	CheckDupe = Utils->FindServer(sid);
	if (CheckDupe)
	{
		socket->SendError("Server ID "+sid+" already exists! You may want to specify the server ID for the server manually with <server:id> so they do not conflict.");
		ServerInstance->SNO.WriteToSnoMask('L', "Server \002"+servername+"\002 being introduced from \002" + ParentOfThis->GetName() + "\002 denied, server ID already exists on the network. Closing link with " + ParentOfThis->GetName());
		return CmdResult::FAILURE;
	}

	TreeServer* route = ParentOfThis->GetRoute();
	std::shared_ptr<Link> lnk = Utils->FindLink(route->GetName());
	auto* Node = new TreeServer(servername, description, sid, ParentOfThis, ParentOfThis->GetSocket(), lnk ? lnk->Hidden : false);

	HandleExtra(Node, params);

	ServerInstance->SNO.WriteToSnoMask('L', "Server \002"+ParentOfThis->GetName()+"\002 introduced server \002"+servername+"\002 ("+description+")");
	return CmdResult::SUCCESS;
}

void CommandServer::HandleExtra(TreeServer* newserver, Params& params)
{
	for (CommandBase::Params::const_iterator i = params.begin() + 2; i != params.end() - 1; ++i)
	{
		const std::string& prop = *i;
		std::string::size_type p = prop.find('=');

		std::string key = prop;
		std::string val;
		if (p != std::string::npos)
		{
			key.erase(p);
			val.assign(prop, p+1, std::string::npos);
		}

		if (irc::equals(key, "burst"))
			newserver->BeginBurst(ConvToNum<uint64_t>(val));
		else if (irc::equals(key, "hidden"))
			newserver->Hidden = !!ConvToNum<uint8_t>(val);
	}
}

std::shared_ptr<Link> TreeSocket::AuthRemote(const CommandBase::Params& params)
{
	if (params.size() < 4)
	{
		SendError("Protocol error - Not enough parameters for SERVER command");
		return nullptr;
	}

	const std::string& sname = params[0];
	const std::string& password = params[1];
	const std::string& sid = params[2];
	const std::string& description = params.back();

	this->SendCapabilities(2);

	if (!InspIRCd::IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return nullptr;
	}

	for (const auto& x : Utils->LinkBlocks)
	{
		if (!InspIRCd::Match(sname, x->Name))
			continue;

		if (!ComparePass(*x, password))
		{
			ServerInstance->SNO.WriteToSnoMask('l', "Invalid password on link: {}", x->Name);
			continue;
		}

		if (!CheckDuplicate(sname, sid))
			return nullptr;

		const SSLIOHook* const ssliohook = SSLIOHook::IsSSL(this);
		if (ssliohook)
		{
			std::string ciphersuite;
			ssliohook->GetCiphersuite(ciphersuite);
			ServerInstance->SNO.WriteToSnoMask('l', "Negotiated ciphersuite {} on link {}", ciphersuite, x->Name);
		}
		else if (!capab->remotesa.is_local() && !RunningInContainer())
		{
			this->SendError("Non-local server connections MUST be linked with SSL!");
			return nullptr;
		}

		ServerInstance->SNO.WriteToSnoMask('l', "Verified server connection " + linkID + " ("+description+")");
		return x;
	}

	this->SendError("Mismatched server name or password (check the other server's snomask output for details - e.g. user mode +s +Ll)");
	ServerInstance->SNO.WriteToSnoMask('l', "Server connection from \002"+sname+"\002 denied, invalid link credentials");
	return nullptr;
}

/*
 * This is used after the other side of a connection has accepted our credentials.
 * They are then introducing themselves to us, BEFORE either of us burst. -- w
 */
bool TreeSocket::Outbound_Reply_Server(CommandBase::Params& params)
{
	const std::shared_ptr<Link> x = AuthRemote(params);
	if (x)
	{
		/*
		 * They're in WAIT_AUTH_2 (having accepted our credentials).
		 * Set our state to CONNECTED (since everything's peachy so far) and send our
		 * netburst to them, which will trigger their CONNECTED state, and BURST in reply.
		 *
		 * While we're at it, create a treeserver object so we know about them.
		 *   -- w
		 */
		FinishAuth(params[0], params[2], params.back(), x->Hidden);

		return true;
	}

	return false;
}

bool TreeSocket::CheckDuplicate(const std::string& sname, const std::string& sid)
{
	// Check if the server name is not in use by a server that's already fully connected
	TreeServer* CheckDupe = Utils->FindServer(sname);
	if (CheckDupe)
	{
		std::string pname = CheckDupe->GetTreeParent() ? CheckDupe->GetTreeParent()->GetName() : "<ourself>";
		SendError("Server "+sname+" already exists on server "+pname+"!");
		ServerInstance->SNO.WriteToSnoMask('l', "Server connection from \002"+sname+"\002 denied, already exists on server "+pname);
		return false;
	}

	// Check if the id is not in use by a server that's already fully connected
	ServerInstance->Logs.Debug(MODNAME, "Looking for dupe SID {}", sid);
	CheckDupe = Utils->FindServerID(sid);

	if (CheckDupe)
	{
		this->SendError("Server ID "+CheckDupe->GetId()+" already exists on server "+CheckDupe->GetName()+"! You may want to specify the server ID for the server manually with <server:id> so they do not conflict.");
		ServerInstance->SNO.WriteToSnoMask('l', "Server connection from \002"+sname+"\002 denied, server ID '"+CheckDupe->GetId()+
				"' already exists on server "+CheckDupe->GetName());
		return false;
	}

	return true;
}

/*
 * Someone else is attempting to connect to us if this is called. Validate their credentials etc.
 *		-- w
 */
bool TreeSocket::Inbound_Server(CommandBase::Params& params)
{
	const std::shared_ptr<Link> x = AuthRemote(params);
	if (x)
	{
		// Save these for later, so when they accept our credentials (indicated by BURST) we remember them
		this->capab->hidden = x->Hidden;
		this->capab->sid = params[2];
		this->capab->description = params.back();
		this->capab->name = params[0];

		// Send our details: Our server name and description and hopcount of 0,
		// along with the sendpass from this block.
		this->WriteLine(FMT::format("SERVER {} {} {} :{}",
			ServerInstance->Config->ServerName,
			TreeSocket::MakePass(x->SendPass, this->GetTheirChallenge()),
			ServerInstance->Config->ServerId,
			ServerInstance->Config->ServerDesc
		));

		// move to the next state, we are now waiting for THEM.
		this->LinkState = WAIT_AUTH_2;
		return true;
	}

	return false;
}

CommandServer::Builder::Builder(TreeServer* server)
	: CmdBuilder(server->GetTreeParent(), "SERVER")
{
	push(server->GetName());
	push(server->GetId());
	if (server->IsBursting())
		push_property("burst", ConvToStr(server->StartBurst));
	push_property("hidden", ConvToStr(server->Hidden));
	push_last(server->GetDesc());
}
