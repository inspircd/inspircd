/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2018-2020, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008, 2012 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
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
#include "timeutils.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"
#include "resolvers.h"
#include "commands.h"

/* Handle ERROR command */
void TreeSocket::Error(CommandBase::Params& params)
{
	const std::string msg = params.empty() ? "" : params[0];
	SetError("received ERROR " + msg);
}

void TreeSocket::Split(const std::string& line, std::string& tags, std::string& prefix, std::string& command, CommandBase::Params& params)
{
	std::string token;
	irc::tokenstream tokens(line);

	if (!tokens.GetMiddle(token))
		return;

	if (token[0] == '@')
	{
		if (token.length() <= 1)
		{
			this->SendError("BUG: Received a message with empty tags: " + line);
			return;
		}

		tags.assign(token, 1, std::string::npos);
		if (!tokens.GetMiddle(token))
		{
			this->SendError("BUG: Received a message with no command: " + line);
			return;
		}
	}

	if (token[0] == ':')
	{
		if (token.length() <= 1)
		{
			this->SendError("BUG: Received a message with an empty prefix: " + line);
			return;
		}

		prefix.assign(token, 1, std::string::npos);
		if (!tokens.GetMiddle(token))
		{
			this->SendError("BUG: Received a message with no command: " + line);
			return;
		}
	}

	command.assign(token);
	while (tokens.GetTrailing(token))
		params.push_back(token);
}

void TreeSocket::ProcessLine(std::string& line)
{
	std::string tags;
	std::string prefix;
	std::string command;
	CommandBase::Params params;

	ServerInstance->Logs.RawIO(MODNAME, "S[{}] I {}", GetFd(), line);

	Split(line, tags, prefix, command, params);

	if (command.empty())
		return;

	switch (this->LinkState)
	{
		case WAIT_AUTH_1:
			/*
			 * State WAIT_AUTH_1:
			 *  Waiting for SERVER command from remote server. Server initiating
			 *  the connection sends the first SERVER command, listening server
			 *  replies with theirs if its happy, then if the initiator is happy,
			 *  it starts to send its net sync, which starts the merge, otherwise
			 *  it sends an ERROR.
			 */
			if (command == "PASS")
			{
				/*
				 * Ignore this silently. Some services packages insist on sending PASS, even
				 * when it is not required (i.e. by us). We have to ignore this here, otherwise
				 * as it's an unknown command (effectively), it will cause the connection to be
				 * closed, which probably isn't what people want. -- w00t
				 */
			}
			else if (command == "SERVER")
			{
				this->Inbound_Server(params);
			}
			else if (command == "ERROR")
			{
				this->Error(params);
			}
			else if (command == "USER")
			{
				this->SendError("Client connections to this port are prohibited.");
			}
			else if (command == "CAPAB")
			{
				this->Capab(params);
			}
			else
			{
				this->SendError("Invalid command in negotiation phase: " + command);
			}
		break;
		case WAIT_AUTH_2:
			/*
			 * State WAIT_AUTH_2:
			 *  We have sent SERVER to the other side of the connection. Now we're waiting for them to start BURST.
			 *  The other option at this stage of things, of course, is for them to close our connection thanks
			 *  to invalid credentials.. -- w
			 */
			if (command == "SERVER")
			{
				/*
				 * Connection is either attempting to re-auth itself (stupid) or sending netburst without sending BURST.
				 * Both of these aren't allowable, so block them here. -- w
				 */
				this->SendError("You may not re-authenticate or commence netburst without sending BURST.");
			}
			else if (command == "BURST")
			{
				if (!params.empty())
				{
					time_t them = ServerCommand::ExtractTS(params[0]);
					time_t delta = them - ServerInstance->Time();
					if ((delta < -15) || (delta > 15))
					{
						ServerInstance->SNO.WriteGlobalSno('l', "\002ERROR\002: Your clocks are off by {} seconds (this is more than fifteen seconds). Link aborted, \002PLEASE SYNC YOUR CLOCKS!\002", labs((long)delta));
						SendError("Your clocks are out by "+ConvToStr(labs((long)delta))+" seconds (this is more than fifteen seconds). Link aborted, PLEASE SYNC YOUR CLOCKS!");
						return;
					}
					else if ((delta < -5) || (delta > 5))
					{
						ServerInstance->SNO.WriteGlobalSno('l', "\002WARNING\002: Your clocks are off by {} seconds. Please consider syncing your clocks.", labs((long)delta));
					}
				}

				// Check for duplicate server name/sid again, it's possible that a new
				// server was introduced while we were waiting for them to send BURST.
				// (we do not reserve their server name/sid when they send SERVER, we do it now)
				if (!CheckDuplicate(capab->name, capab->sid))
					return;

				FinishAuth(capab->name, capab->sid, capab->description, capab->hidden);
			}
			else if (command == "ERROR")
			{
				this->Error(params);
			}
			else if (command == "CAPAB")
			{
				this->Capab(params);
			}
			else
			{
				ServerInstance->Logs.Debug(MODNAME, "Unknown command from fd {} in the WAIT_AUTH_2 phase: {}",
					GetFd(), command);
			}

		break;
		case CONNECTING:
			/*
			 * State CONNECTING:
			 *  We're connecting (OUTGOING) to another server. They are in state WAIT_AUTH_1 until they verify
			 *  our credentials, when they proceed into WAIT_AUTH_2 and send SERVER to us. We then send BURST
			 *  + our netburst, which will put them into CONNECTED state. -- w
			 */
			if (command == "SERVER")
			{
				// Our credentials have been accepted, send netburst. (this puts US into the CONNECTED state)
				this->Outbound_Reply_Server(params);
			}
			else if (command == "ERROR")
			{
				this->Error(params);
			}
			else if (command == "CAPAB")
			{
				this->Capab(params);
			}
			else
			{
				ServerInstance->Logs.Debug(MODNAME, "Unknown command from fd {} in the CONNECTING phase: {}",
					GetFd(), command);
			}
		break;

		case CONNECTED:
			/*
			 * State CONNECTED:
			 *  Credentials have been exchanged, we've gotten their 'BURST' (or sent ours).
			 *  Anything from here on should be accepted a little more reasonably.
			 */
			this->ProcessConnectedLine(tags, prefix, command, params);
		break;

		case DYING:
		break;
	}
}

User* TreeSocket::FindSource(const std::string& prefix, const std::string& command)
{
	// Empty prefix means the source is the directly connected server that sent this command
	if (prefix.empty())
		return MyRoot->ServerUser;

	if (prefix.size() == 3)
	{
		// Prefix looks like a sid
		TreeServer* server = Utils->FindServerID(prefix);
		if (server)
			return server->ServerUser;
	}
	else
	{
		// If the prefix string is a uuid FindUUID() returns the appropriate User object
		auto* user = ServerInstance->Users.FindUUID(prefix);
		if (user)
			return user;
	}

	// Some implementations wrongly send a server name as prefix occasionally, handle that too for now
	TreeServer* const server = Utils->FindServer(prefix);
	if (server)
		return server->ServerUser;

	/* It is important that we don't close the link here, unknown prefix can occur
	 * due to various race conditions such as the KILL message for a user somehow
	 * crossing the users QUIT further upstream from the server. Thanks jilles!
	 */

	if ((prefix.length() == UIDGenerator::UUID_LENGTH) && (isdigit(prefix[0])) &&
		((command == "FMODE") || (command == "MODE") || (command == "KICK") || (command == "TOPIC") || (command == "KILL") || (command == "ADDLINE") || (command == "DELLINE")))
	{
		/* Special case, we cannot drop these commands as they've been committed already on a
		 * part of the network by the time we receive them, so in this scenario pretend the
		 * command came from a server to avoid desync.
		 */

		TreeServer* const usersserver = Utils->FindServerID(prefix.substr(0, 3));
		if (usersserver)
			return usersserver->ServerUser;
		return this->MyRoot->ServerUser;
	}

	// Unknown prefix
	return nullptr;
}

void TreeSocket::ProcessTag(User* source, const std::string& tag, ClientProtocol::TagMap& tags)
{
	std::string tagkey;
	std::string tagval;
	const std::string::size_type p = tag.find('=');
	if (p != std::string::npos)
	{
		// Tag has a value
		tagkey.assign(tag, 0, p);
		tagval.assign(tag, p + 1, std::string::npos);
	}
	else
	{
		tagkey.assign(tag);
	}

	for (auto* subscriber : Utils->Creator->tagevprov.GetSubscribers())
	{
		ClientProtocol::MessageTagProvider* const tagprov = static_cast<ClientProtocol::MessageTagProvider*>(subscriber);
		const ModResult res = tagprov->OnProcessTag(source, tagkey, tagval);
		if (res == MOD_RES_ALLOW)
			tags.emplace(tagkey, ClientProtocol::MessageTagData(tagprov, tagval));
		else if (res == MOD_RES_DENY)
			break;
	}
}

void TreeSocket::ProcessConnectedLine(std::string& taglist, std::string& prefix, std::string& command, CommandBase::Params& params)
{
	User* who = FindSource(prefix, command);
	if (!who)
	{
		ServerInstance->Logs.Debug(MODNAME, "Command '{}' from unknown prefix '{}'! Dropping entire command.", command, prefix);
		return;
	}

	/*
	 * Check for fake direction here, and drop any instances that are found.
	 * What is fake direction? Imagine the following server setup:
	 *    0AA <-> 0AB <-> 0AC
	 * Fake direction would be 0AC sending a message to 0AB claiming to be from
	 * 0AA, or something similar. Basically, a message taking a path that *cannot*
	 * be correct.
	 *
	 * When would this be seen?
	 * Well, hopefully never. It could be caused by race conditions, bugs, or
	 * "miscreant" servers, though, so let's check anyway. -- w
	 *
	 * We also check here for totally invalid prefixes (prefixes that are neither
	 * a valid SID or a valid UUID, so that invalid UUID or SID never makes it
	 * to the higher level functions. -- B
	 */
	TreeServer* const server = TreeServer::Get(who);
	if (server->GetSocket() != this)
	{
		ServerInstance->Logs.Debug(MODNAME, "Protocol violation: Fake direction '{}' from connection '{}'", prefix, linkID);
		return;
	}

	// Translate commands coming from servers using an older protocol
	if (proto_version < PROTO_NEWEST)
	{
		if (!PreProcessOldProtocolMessage(who, command, params))
			return;
	}

	ServerCommand* scmd = Utils->Creator->CmdManager.GetHandler(command);
	CommandBase* cmdbase = scmd;
	Command* cmd = nullptr;
	if (!scmd)
	{
		// Not a special server-to-server command
		cmd = ServerInstance->Parser.GetHandler(command);
		if (!cmd)
		{
			if (command == "ERROR")
			{
				this->Error(params);
				return;
			}
			else if (command == "BURST")
			{
				// This is sent even when there is no need for it, drop it here for now
				return;
			}

			throw ProtocolException("Unknown command: " + command);
		}
		cmdbase = cmd;
	}

	if (params.size() < cmdbase->min_params)
		throw ProtocolException("Insufficient parameters");

	if ((!params.empty()) && (params.back().empty()) && (!cmdbase->allow_empty_last_param))
	{
		// the last param is empty and the command handler doesn't allow that, check if there will be enough params if we drop the last
		if (params.size()-1 < cmdbase->min_params)
			return;
		params.pop_back();
	}

	CmdResult res;
	ClientProtocol::TagMap tags;
	std::string tag;
	irc::sepstream tagstream(taglist, ';');
	while (tagstream.GetToken(tag))
		ProcessTag(who, tag, tags);

	CommandBase::Params newparams(params, tags);

	if (scmd)
		res = scmd->Handle(who, newparams);
	else
	{
		res = cmd->Handle(who, newparams);
		if (res == CmdResult::INVALID)
			throw ProtocolException("Error in command handler");
	}

	if (res == CmdResult::SUCCESS)
		Utils->RouteCommand(server->GetRoute(), cmdbase, newparams, who);
}

void TreeSocket::OnTimeout()
{
	ServerInstance->SNO.WriteGlobalSno('l', "CONNECT: Connection to \002{}\002 timed out.", linkID);
}

void TreeSocket::Close()
{
	if (!HasFd())
		return;

	ServerInstance->GlobalCulls.AddItem(this);
	this->BufferedSocket::Close();
	SetError("Remote host closed connection");

	// Connection closed.
	// If the connection is fully up (state CONNECTED)
	// then propagate a netsplit to all peers.
	if (MyRoot && !MyRoot->IsDead())
		MyRoot->SQuit(GetError(), true);
	else
		ServerInstance->SNO.WriteGlobalSno('l', "Connection to '\002{}\002' failed.", linkID);

	time_t server_uptime = ServerInstance->Time() - this->age;
	if (server_uptime)
	{
		std::string timestr = Duration::ToString(server_uptime);
		ServerInstance->SNO.WriteGlobalSno('l', "Connection to '\002{}\002' was established for {}", linkID, timestr);
	}
}

void TreeSocket::FinishAuth(const std::string& remotename, const std::string& remotesid, const std::string& remotedesc, bool hidden)
{
	this->LinkState = CONNECTED;
	Utils->timeoutlist.erase(this);

	linkID = remotename;

	MyRoot = new TreeServer(remotename, remotedesc, remotesid, Utils->TreeRoot, this, hidden);

	// Mark the server as bursting
	MyRoot->BeginBurst();
	this->DoBurst(MyRoot);

	CommandServer::Builder(MyRoot).Forward(MyRoot);
}
