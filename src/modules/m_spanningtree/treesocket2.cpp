/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008, 2012 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"
#include "resolvers.h"
#include "commands.h"

/* Handle ERROR command */
void TreeSocket::Error(parameterlist &params)
{
	std::string msg = params.size() ? params[0] : "";
	SetError("received ERROR " + msg);
}

void TreeSocket::Split(const std::string& line, std::string& prefix, std::string& command, parameterlist& params)
{
	irc::tokenstream tokens(line);

	if (!tokens.GetToken(prefix))
		return;

	if (prefix[0] == ':')
	{
		prefix.erase(prefix.begin());

		if (prefix.empty())
		{
			this->SendError("BUG (?) Empty prefix received: " + line);
			return;
		}
		if (!tokens.GetToken(command))
		{
			this->SendError("BUG (?) Empty command received: " + line);
			return;
		}
	}
	else
	{
		command = prefix;
		prefix.clear();
	}
	if (command.empty())
		this->SendError("BUG (?) Empty command received: " + line);

	std::string param;
	while (tokens.GetToken(param))
	{
		params.push_back(param);
	}
}

void TreeSocket::ProcessLine(std::string &line)
{
	std::string prefix;
	std::string command;
	parameterlist params;

	ServerInstance->Logs->Log(MODNAME, LOG_RAWIO, "S[%d] I %s", this->GetFd(), line.c_str());

	Split(line, prefix, command, params);

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
				if (params.size())
				{
					time_t them = ConvToInt(params[0]);
					time_t delta = them - ServerInstance->Time();
					if ((delta < -600) || (delta > 600))
					{
						ServerInstance->SNO->WriteGlobalSno('l',"\2ERROR\2: Your clocks are out by %ld seconds (this is more than five minutes). Link aborted, \2PLEASE SYNC YOUR CLOCKS!\2",labs((long)delta));
						SendError("Your clocks are out by "+ConvToStr(labs((long)delta))+" seconds (this is more than five minutes). Link aborted, PLEASE SYNC YOUR CLOCKS!");
						return;
					}
					else if ((delta < -30) || (delta > 30))
					{
						ServerInstance->SNO->WriteGlobalSno('l',"\2WARNING\2: Your clocks are out by %ld seconds. Please consider synching your clocks.", labs((long)delta));
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
		break;
		case CONNECTED:
			/*
			 * State CONNECTED:
			 *  Credentials have been exchanged, we've gotten their 'BURST' (or sent ours).
			 *  Anything from here on should be accepted a little more reasonably.
			 */
			this->ProcessConnectedLine(prefix, command, params);
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

	// If the prefix string is a uuid or a sid FindUUID() returns the appropriate User object
	User* who = ServerInstance->FindUUID(prefix);
	if (who)
		return who;

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

		who = ServerInstance->FindUUID(prefix.substr(0, 3));
		if (who)
			return who;
		return this->MyRoot->ServerUser;
	}

	// Unknown prefix
	return NULL;
}

void TreeSocket::ProcessConnectedLine(std::string& prefix, std::string& command, parameterlist& params)
{
	User* who = FindSource(prefix, command);
	if (!who)
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Command '%s' from unknown prefix '%s'! Dropping entire command.", command.c_str(), prefix.c_str());
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
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Protocol violation: Fake direction '%s' from connection '%s'", prefix.c_str(), linkID.c_str());
		return;
	}

	// Translate commands coming from servers using an older protocol
	if (proto_version < ProtocolVersion)
	{
		if (!PreProcessOldProtocolMessage(who, command, params))
			return;
	}

	ServerCommand* scmd = Utils->Creator->CmdManager.GetHandler(command);
	CommandBase* cmdbase = scmd;
	Command* cmd = NULL;
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

			throw ProtocolException("Unknown command");
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
	if (scmd)
		res = scmd->Handle(who, params);
	else
	{
		res = cmd->Handle(params, who);
		if (res == CMD_INVALID)
			throw ProtocolException("Error in command handler");
	}

	if (res == CMD_SUCCESS)
		Utils->RouteCommand(server->GetRoute(), cmdbase, params, who);
}

void TreeSocket::OnTimeout()
{
	ServerInstance->SNO->WriteGlobalSno('l', "CONNECT: Connection to \002%s\002 timed out.", linkID.c_str());
}

void TreeSocket::Close()
{
	if (fd < 0)
		return;

	ServerInstance->GlobalCulls.AddItem(this);
	this->BufferedSocket::Close();
	SetError("Remote host closed connection");

	// Connection closed.
	// If the connection is fully up (state CONNECTED)
	// then propogate a netsplit to all peers.
	if (MyRoot)
		MyRoot->SQuit(getError());

	ServerInstance->SNO->WriteGlobalSno('l', "Connection to '\2%s\2' failed.",linkID.c_str());

	time_t server_uptime = ServerInstance->Time() - this->age;
	if (server_uptime)
	{
		std::string timestr = ModuleSpanningTree::TimeToStr(server_uptime);
		ServerInstance->SNO->WriteGlobalSno('l', "Connection to '\2%s\2' was established for %s", linkID.c_str(), timestr.c_str());
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
