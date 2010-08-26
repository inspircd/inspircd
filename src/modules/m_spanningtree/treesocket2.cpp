/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "resolvers.h"

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
		prefix = prefix.substr(1);

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

void TreeSocket::ProcessLine(const std::string &line)
{
	std::string prefix;
	std::string command;
	parameterlist params;
	CrashState cmd_tracer(HERE_STR, line.c_str());

	ServerInstance->Logs->Log("m_spanningtree", RAWIO, "S[%d] I %s", this->GetFd(), line.c_str());

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
				this->SendError(std::string("Invalid command in negotiation phase: ") + command.c_str());
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
					time_t them = atoi(params[0].c_str());
					time_t delta = them - ServerInstance->Time();
					if ((delta < -600) || (delta > 600))
					{
						ServerInstance->SNO->WriteGlobalSno('l',"\2ERROR\2: Your clocks are out by %d seconds (this is more than five minutes). Link aborted, \2PLEASE SYNC YOUR CLOCKS!\2",abs((long)delta));
						SendError("Your clocks are out by "+ConvToStr(abs((long)delta))+" seconds (this is more than five minutes). Link aborted, PLEASE SYNC YOUR CLOCKS!");
						return;
					}
					else if ((delta < -30) || (delta > 30))
					{
						ServerInstance->SNO->WriteGlobalSno('l',"\2WARNING\2: Your clocks are out by %d seconds. Please consider synching your clocks.", abs((long)delta));
					}
				}
				this->LinkState = CONNECTED;

				Utils->timeoutlist.erase(this);
				parameterlist sparams;
				Utils->DoOneToAllButSender(MyRoot->GetID(), "BURST", params, MyRoot->GetName());
				MyRoot->bursting = true;
				this->DoBurst(MyRoot);
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

void TreeSocket::ProcessConnectedLine(std::string& prefix, std::string& command, parameterlist& params)
{
	User* who = ServerInstance->FindUUID(prefix);
	std::string direction;

	if (!who)
	{
		TreeServer* ServerSource = Utils->FindServer(prefix);
		if (prefix.empty())
			ServerSource = MyRoot;

		if (ServerSource)
		{
			who = ServerSource->ServerUser;
		}
		else
		{
			/* It is important that we don't close the link here, unknown prefix can occur
			 * due to various race conditions such as the KILL message for a user somehow
			 * crossing the users QUIT further upstream from the server. Thanks jilles!
			 */
			ServerInstance->Logs->Log("m_spanningtree", DEBUG, "Command '%s' from unknown prefix '%s'! Dropping entire command.",
				command.c_str(), prefix.c_str());
			return;
		}
	}

	// Make sure prefix is still good
	direction = who->server;
	prefix = who->uuid;

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
	TreeServer* route_back_again = Utils->BestRouteTo(direction);
	if ((!route_back_again) || (route_back_again->GetSocket() != this))
	{
		if (route_back_again)
			ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Protocol violation: Fake direction '%s' from connection '%s'",
				prefix.c_str(),linkID.c_str());
		return;
	}

	/*
	 * First up, check for any malformed commands (e.g. MODE without a timestamp)
	 * and rewrite commands where necessary (SVSMODE -> MODE for services). -- w
	 */
	if (command == "SVSMODE") // This isn't in an "else if" so we still force FMODE for changes on channels.
		command = "MODE";

	// TODO move all this into Commands
	if (command == "MAP")
	{
		Utils->Creator->HandleMap(params, who);
	}
	else if (command == "SERVER")
	{
		this->RemoteServer(prefix,params);
	}
	else if (command == "ERROR")
	{
		this->Error(params);
	}
	else if (command == "AWAY")
	{
		this->Away(prefix,params);
	}
	else if (command == "RESYNC" && !params.empty())
	{
		Channel* chan = ServerInstance->FindChan(params[0]);
		if (chan)
			SendFJoins(chan);
	}
	else if (command == "PING")
	{
		this->LocalPing(prefix,params);
	}
	else if (command == "PONG")
	{
		TreeServer *s = Utils->FindServer(prefix);
		if (s && s->bursting)
		{
			ServerInstance->SNO->WriteGlobalSno('l',"Server \002%s\002 has not finished burst, forcing end of burst (send ENDBURST!)", prefix.c_str());
			s->FinishBurst();
		}
		this->LocalPong(prefix,params);
	}
	else if (command == "VERSION")
	{
		this->ServerVersion(prefix,params);
	}
	else if (command == "ADDLINE")
	{
		this->AddLine(prefix,params);
	}
	else if (command == "DELLINE")
	{
		this->DelLine(prefix,params);
	}
	else if (command == "SAVE")
	{
		this->ForceNick(prefix,params);
	}
	else if (command == "OPERQUIT")
	{
		this->OperQuit(prefix,params);
	}
	else if (command == "IDLE")
	{
		this->Whois(prefix,params);
	}
	else if (command == "PUSH")
	{
		this->Push(prefix,params);
	}
	else if (command == "SQUIT")
	{
		if (params.size() == 2)
		{
			this->Squit(Utils->FindServer(params[0]),params[1]);
		}
	}
	else if (command == "SNONOTICE")
	{
		if (params.size() >= 2)
		{
			ServerInstance->SNO->WriteToSnoMask(params[0][0], "From " + who->nick + ": "+ params[1]);
			params[1] = ":" + params[1];
			Utils->DoOneToAllButSender(prefix, command, params, prefix);
		}
	}
	else if (command == "BURST")
	{
		// Set prefix server as bursting
		TreeServer* ServerSource = Utils->FindServer(prefix);
		if (!ServerSource)
		{
			ServerInstance->SNO->WriteGlobalSno('l', "WTF: Got BURST from a non-server(?): %s", prefix.c_str());
			return;
		}

		ServerSource->bursting = true;
		Utils->DoOneToAllButSender(prefix, command, params, prefix);
	}
	else if (command == "ENDBURST")
	{
		TreeServer* ServerSource = Utils->FindServer(prefix);
		if (!ServerSource)
		{
			ServerInstance->SNO->WriteGlobalSno('l', "WTF: Got ENDBURST from a non-server(?): %s", prefix.c_str());
			return;
		}

		ServerSource->FinishBurst();
		Utils->DoOneToAllButSender(prefix, command, params, prefix);
	}
	else if (command == "ENCAP")
	{
		this->Encap(who, params);
	}
	else if (command == "NICK")
	{
		if (params.size() != 2)
		{
			SendError("Protocol violation: NICK message without TS - :"+std::string(who->uuid)+" NICK "+params[0]);
			return;
		}

		if (IS_SERVER(who))
		{
			SendError("Protocol violation: Server changing nick");
			return;
		}

		/* Update timestamp on user when they change nicks */
		who->age = atoi(params[1].c_str());

		/*
		 * On nick messages, check that the nick doesnt already exist here.
		 * If it does, perform collision logic.
		 */
		User* x = ServerInstance->FindNickOnly(params[0]);
		if ((x) && (x != who))
		{
			int collideret = 0;
			/* x is local, who is remote */
			collideret = this->DoCollision(x, who->age, who->ident, who->GetIPString(), who->uuid);
			if (collideret != 1)
			{
				/*
				 * Remote client lost, or both lost, parsing or passing on this
				 * nickchange would be pointless, as the incoming client's server will
				 * soon recieve SVSNICK to change its nick to its UID. :) -- w00t
				 */
				return;
			}
		}
		who->ForceNickChange(params[0].c_str());
		Utils->RouteCommand(route_back_again, command, params, who);
	}
	else
	{
		Command* cmd = ServerInstance->Parser->GetHandler(command);
		CmdResult res = CMD_INVALID;
		if (cmd && params.size() >= cmd->min_params)
		{
			res = cmd->Handle(params, who);
		}

		if (res == CMD_INVALID)
		{
			irc::stringjoiner pmlist(" ", params, 0, params.size() - 1);
			ServerInstance->Logs->Log("spanningtree", SPARSE, "Invalid S2S command :%s %s %s",
				who->uuid.c_str(), command.c_str(), pmlist.GetJoined().c_str());
			SendError("Unrecognised or malformed command '" + command + "' -- possibly loaded mismatched modules");
		}
		if (res == CMD_SUCCESS)
			Utils->RouteCommand(route_back_again, command, params, who);
	}
}

void TreeSocket::OnTimeout()
{
	ServerInstance->SNO->WriteGlobalSno('l', "CONNECT: Connection to \002%s\002 timed out.", linkID.c_str());
}

void TreeSocket::Close()
{
	if (fd != -1)
		ServerInstance->GlobalCulls.AddItem(this);
	this->BufferedSocket::Close();
	SetError("Remote host closed connection");

	// Connection closed.
	// If the connection is fully up (state CONNECTED)
	// then propogate a netsplit to all peers.
	if (MyRoot)
		Squit(MyRoot,getError());

	if (!linkID.empty())
	{
		ServerInstance->SNO->WriteGlobalSno('l', "Connection to '\2%s\2' failed.",linkID.c_str());

		time_t server_uptime = ServerInstance->Time() - this->age;
		if (server_uptime)
			ServerInstance->SNO->WriteGlobalSno('l', "Connection to '\2%s\2' was established for %s", linkID.c_str(), Utils->Creator->TimeToStr(server_uptime).c_str());
		linkID.clear();
	}
}
