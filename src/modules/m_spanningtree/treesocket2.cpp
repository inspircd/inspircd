/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_spanningtree/handshaketimer.h */

void TreeSocket::WriteLine(std::string line)
{
	if (LinkState == DYING)
		return;
	ServerInstance->Logs->Log("m_spanningtree",DEBUG, "S[%d] O %s", this->GetFd(), line.c_str());
	line.append("\r\n");
	this->Write(line);
}


/* Handle ERROR command */
bool TreeSocket::Error(std::deque<std::string> &params)
{
	if (params.size() < 1)
		return false;
	this->ServerInstance->SNO->WriteToSnoMask('l',"ERROR from %s: %s",(!InboundServerName.empty() ? InboundServerName.c_str() : myhost.c_str()),params[0].c_str());
	/* we will return false to cause the socket to close. */
	return false;
}

void TreeSocket::Split(const std::string &line, std::deque<std::string> &n)
{
	n.clear();
	irc::tokenstream tokens(line);
	std::string param;
	while (tokens.GetToken(param))
	{
		n.push_back(param);
	}
	return;
}

bool TreeSocket::ProcessLine(std::string &line)
{
	std::deque<std::string> params;
	irc::string command;
	std::string prefix;

	line = line.substr(0, line.find_first_of("\r\n"));

	if (line.empty())
		return true;

	ServerInstance->Logs->Log("m_spanningtree",DEBUG, "S[%d] I %s", this->GetFd(), line.c_str());

	this->Split(line.c_str(),params);

	if (params.empty())
		return true;

	if ((params[0][0] == ':') && (params.size() > 1))
	{
		prefix = params[0].substr(1);
		params.pop_front();

		if (prefix.empty())
		{
			this->SendError("BUG (?) Empty prefix recieved: " + line);
			return false;
		}
	}

	command = params[0].c_str();
	params.pop_front();

	switch (this->LinkState)
	{
		TreeServer* Node;

		case DYING:
			return false;
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
				return this->Inbound_Server(params);
			}
			else if (command == "ERROR")
			{
				return this->Error(params);
			}
			else if (command == "USER")
			{
				this->SendError("Client connections to this port are prohibited.");
				return false;
			}
			else if (command == "CAPAB")
			{
				return this->Capab(params);
			}
			else
			{
				// XXX ...wtf.
				irc::string error = "Invalid command in negotiation phase: " + command;
				this->SendError(assign(error));
				return false;
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
				return true;
			}
			else if (command == "BURST")
			{
				if (params.size())
				{
					time_t them = atoi(params[0].c_str());
					time_t delta = them - ServerInstance->Time();
					if ((delta < -600) || (delta > 600))
					{
						ServerInstance->SNO->WriteToSnoMask('l',"\2ERROR\2: Your clocks are out by %d seconds (this is more than five minutes). Link aborted, \2PLEASE SYNC YOUR CLOCKS!\2",abs((long)delta));
						SendError("Your clocks are out by "+ConvToStr(abs((long)delta))+" seconds (this is more than five minutes). Link aborted, PLEASE SYNC YOUR CLOCKS!");
						return false;
					}
					else if ((delta < -30) || (delta > 30))
					{
						ServerInstance->SNO->WriteToSnoMask('l',"\2WARNING\2: Your clocks are out by %d seconds. Please consider synching your clocks.", abs((long)delta));
					}
				}
				this->LinkState = CONNECTED;

				Utils->timeoutlist.erase(this);

				Link* lnk = Utils->FindLink(InboundServerName);

				Node = new TreeServer(this->Utils, this->ServerInstance, InboundServerName, InboundDescription, InboundSID, Utils->TreeRoot, this, lnk ? lnk->Hidden : false);

				Utils->TreeRoot->AddChild(Node);
				parameterlist sparams;
				sparams.push_back(InboundServerName);
				sparams.push_back("*");
				sparams.push_back("1");
				sparams.push_back(InboundSID);
				sparams.push_back(":"+InboundDescription);
				Utils->DoOneToAllButSender(ServerInstance->Config->GetSID(),"SERVER",sparams,InboundServerName);
				Utils->DoOneToAllButSenderRaw(line, InboundServerName, prefix, command, params);
				Node->bursting = true;
				this->DoBurst(Node);
			}
			else if (command == "ERROR")
			{
				return this->Error(params);
			}
			else if (command == "CAPAB")
			{
				return this->Capab(params);
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
				return this->Outbound_Reply_Server(params);
			}
			else if (command == "ERROR")
			{
				return this->Error(params);
			}
			else if (command == "CAPAB")
			{
				return this->Capab(params);
			}
		break;
		case CONNECTED:
			/*
			* State CONNECTED:
			 *  Credentials have been exchanged, we've gotten their 'BURST' (or sent ours).
			 *  Anything from here on should be accepted a little more reasonably.
			 */
			if (!prefix.empty())
			{
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
				std::string direction = prefix;

				User *t = this->ServerInstance->FindUUID(prefix);
				if (t)
				{
					/* Find UID */
					direction = t->server;
				}
				else if (!this->Utils->FindServer(direction))
				{
					/* Find SID */
					ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Protocol violation: Invalid prefix '%s' from connection '%s'", direction.c_str(), this->GetName().c_str());
					return true;
				}

				TreeServer* route_back_again = Utils->BestRouteTo(direction);
				if ((!route_back_again) || (route_back_again->GetSocket() != this))
				{
					if (route_back_again)
						ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Protocol violation: Fake direction in command '%s' from connection '%s'",line.c_str(),this->GetName().c_str());
					return true;
				}
			}
			else
			{
				/*
				 * Empty prefix from a server to server link:
				 *  This is somewhat bad/naughty, so let's set the prefix
				 *  to be the link that we got it from, so we don't break anything. -- w
				 */
				TreeServer* n = Utils->FindServer(GetName());
				if (n)
					prefix = n->GetID();
				else
					prefix = GetName();
			}

			/*
			 * First up, check for any malformed commands (e.g. MODE without a timestamp)
			 * and rewrite commands where necessary (SVSMODE -> MODE for services). -- w
			 */
			if (command == "SVSMODE") // This isn't in an "else if" so we still force FMODE for changes on channels.
				command = "MODE";

			/*
			 * Now, check for (and parse) commands as appropriate. -- w
			 */

			/* Find the server that this command originated from, used in the handlers below */
			TreeServer *ServerSource = Utils->FindServer(prefix);
			if (ServerSource)
			{
				Utils->ServerUser->SetFakeServer(ServerSource->GetName());
				Utils->ServerUser->uuid = ServerSource->GetID();
			}

			/* Find the link we just got this from so we don't bounce it back incorrectly */
			std::string sourceserv = this->myhost;
			if (!this->InboundServerName.empty())
			{
				sourceserv = this->InboundServerName;
			}

			/*
			 * XXX one of these days, this needs to be moved into class Commands.
			 */
			if (command == "UID")
			{
				return this->ParseUID(prefix, params);
			}
			else if (command == "FJOIN")
			{
				return this->ForceJoin(prefix,params);
			}
			else if ((command == "NOTICE" || command == "PRIVMSG") && (Utils->IsServer(prefix)))
			{
				return this->ServerMessage(assign(command), prefix, params, sourceserv);
			}
			else if (command == "STATS")
			{
				return this->Stats(prefix, params);
			}
			else if (command == "MOTD")
			{
				return this->Motd(prefix, params);
			}
			else if (command == "KILL" && ServerSource)
			{
				// Kill from a server
				return this->RemoteKill(prefix,params);
			}
			else if (command == "MODULES")
			{
				return this->Modules(prefix, params);
			}
			else if (command == "ADMIN")
			{
				return this->Admin(prefix, params);
			}
			else if (command == "MAP")
			{
				User* user = ServerInstance->FindNick(prefix);
				if (user)
				{
					std::vector<std::string> p(params.begin(), params.end());
					return Utils->Creator->HandleMap(p, user);
				}
			}
			else if (command == "SERVER")
			{
				return this->RemoteServer(prefix,params);
			}
			else if (command == "ERROR")
			{
				return this->Error(params);
			}
			else if (command == "OPERTYPE")
			{
				return this->OperType(prefix,params);
			}
			else if (command == "FMODE")
			{
				return this->ForceMode(prefix,params);
			}
			else if (command == "FTOPIC")
			{
				return this->ForceTopic(prefix,params);
			}
			else if (command == "METADATA")
			{
				return this->MetaData(prefix,params);
			}
			else if (command == "PING")
			{
				return this->LocalPing(prefix,params);
			}
			else if (command == "PONG")
			{
				TreeServer *s = Utils->FindServer(prefix);
				if (s && s->bursting)
				{
					ServerInstance->SNO->WriteToSnoMask('l',"Server \002%s\002 has not finished burst, forcing end of burst (send ENDBURST!)", prefix.c_str());
					s->FinishBurst();
				}
				return this->LocalPong(prefix,params);
			}
			else if (command == "VERSION")
			{
				return this->ServerVersion(prefix,params);
			}
			else if (command == "FHOST")
			{
				return this->ChangeHost(prefix,params);
			}
			else if (command == "FNAME")
			{
				return this->ChangeName(prefix,params);
			}
			else if (command == "ADDLINE")
			{
				return this->AddLine(prefix,params);
			}
			else if (command == "DELLINE")
			{
				return this->DelLine(prefix,params);
			}
			else if (command == "SVSNICK")
			{
				return this->ForceNick(prefix,params);
			}
			else if (command == "OPERQUIT")
			{
				return this->OperQuit(prefix,params);
			}
			else if (command == "IDLE")
			{
				return this->Whois(prefix,params);
			}
			else if (command == "PUSH")
			{
				return this->Push(prefix,params);
			}
			else if (command == "TIME")
			{
				return this->Time(prefix,params);
			}
			else if (command == "SVSJOIN")
			{
				return this->ServiceJoin(prefix,params);
			}
			else if (command == "SVSPART")
			{
				return this->ServicePart(prefix,params);
			}
			else if (command == "SQUIT")
			{
				if (params.size() == 2)
				{
					this->Squit(Utils->FindServer(params[0]),params[1]);
				}
				return true;
			}
			else if (command == "MODENOTICE")
			{
				if (params.size() >= 2)
				{
					if (ServerSource)
						ServerInstance->Users->WriteMode(params[0].c_str(), WM_AND, "*** From %s: %s", (ServerSource ? ServerSource->GetName().c_str() : prefix.c_str()), params[1].c_str());
				}
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "SNONOTICE")
			{
				if (params.size() >= 2)
				{
					std::string oldprefix;
					if (!ServerSource)
					{
						oldprefix = prefix;
						User *u = ServerInstance->FindNick(prefix);
						if (!u)
							return true;
						prefix = u->nick;
					}

					ServerInstance->SNO->WriteToSnoMask(*(params[0].c_str()), "From " + (ServerSource ? ServerSource->GetName().c_str() : prefix) + ": "+ params[1]);
					prefix = oldprefix;
					return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
				}

			}
			else if (command == "BURST")
			{
				// Set prefix server as bursting
				if (!ServerSource)
				{
					this->ServerInstance->SNO->WriteToSnoMask('l', "WTF: Got BURST from a nonexistant server(?): %s", (ServerSource ? ServerSource->GetName().c_str() : prefix.c_str()));
					return false;
				}

				ServerSource->bursting = true;
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "ENDBURST")
			{
				if (!ServerSource)
				{
					this->ServerInstance->SNO->WriteToSnoMask('l', "WTF: Got ENDBURST from a nonexistant server(?): %s", (ServerSource ? ServerSource->GetName().c_str() : prefix.c_str()));
					return false;
				}

				ServerSource->FinishBurst();
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "ENCAP")
			{
				return this->Encap(prefix, params);
			}
			else
			{
				/*
				 * Not a special s2s command. Emulate the user doing it.
				 * This saves us having a huge ugly command parser again.
				 */
				User* who = this->ServerInstance->FindUUID(prefix);

				if (ServerSource)
				{
					who = Utils->ServerUser;
				}
				else if (!who)
				{
					/* this looks ugly because command is an irc::string
					 * It is important that we dont close the link here, unknown prefix can occur
					 * due to various race conditions such as the KILL message for a user somehow
					 * crossing the users QUIT further upstream from the server. Thanks jilles!
					 */
					ServerInstance->Logs->Log("m_spanningtree", DEBUG, "Command " + std::string(command.c_str()) + " from unknown prefix " + prefix + "! Dropping entire command.");
					return true;
				}

				if (command == "NICK")
				{
					if (params.size() != 2)
					{
						SendError("Protocol violation: NICK message without TS - :"+std::string(who->uuid)+" NICK "+params[0]);
						return false;
					}
					/* Update timestamp on user when they change nicks */
					who->age = atoi(params[1].c_str());

					/*
					 * On nick messages, check that the nick doesnt already exist here.
					 * If it does, perform collision logic.
					 */
					User* x = this->ServerInstance->FindNickOnly(params[0]);
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
							return true;
						}
					}
				}

				// it's a user
				std::vector<std::string> strparams(params.begin(), params.end());

				switch (this->ServerInstance->CallCommandHandler(command.c_str(), strparams, who))
				{
					case CMD_INVALID:
						/*
						 * XXX: command is irc::string, hence ugliness
						 */
						this->SendError("Unrecognised or malformed command '" + std::string(command.c_str()) + "' -- possibly loaded mismatched modules");
						return false;
						break;
					case CMD_FAILURE:
						/*
						 * CMD_LOCALONLY is aliased to CMD_FAILURE, so this won't go out onto the network.
						 */
						return true;
						break;
					default:
						/* CMD_SUCCESS falls through here */
						break;
				}

				return Utils->DoOneToAllButSenderRaw(line,sourceserv,prefix,command,params);

			}
			return true;
	}
	return true;
}

std::string TreeSocket::GetName()
{
	std::string sourceserv = this->myhost;
	if (!this->InboundServerName.empty())
	{
		sourceserv = this->InboundServerName;
	}
	return sourceserv;
}

void TreeSocket::OnTimeout()
{
	if (this->LinkState == CONNECTING)
	{
		this->ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Connection to \002%s\002 timed out.", myhost.c_str());
		Link* MyLink = Utils->FindLink(myhost);
		if (MyLink)
			Utils->DoFailOver(MyLink);
	}
}

void TreeSocket::OnClose()
{
	// Test fix for big fuckup
	if (this->LinkState != CONNECTED)
		return;

	// Connection closed.
	// If the connection is fully up (state CONNECTED)
	// then propogate a netsplit to all peers.
	std::string quitserver = this->myhost;
	if (!this->InboundServerName.empty())
	{
		quitserver = this->InboundServerName;
	}
	TreeServer* s = Utils->FindServer(quitserver);
	if (s)
	{
		Squit(s,"Remote host closed the connection");
	}

	if (!quitserver.empty())
	{
		this->ServerInstance->SNO->WriteToSnoMask('l', "Connection to '\2%s\2' failed.",quitserver.c_str());

		time_t server_uptime = ServerInstance->Time() - this->age;
		if (server_uptime)
				this->ServerInstance->SNO->WriteToSnoMask('l', "Connection to '\2%s\2' was established for %s", quitserver.c_str(), Utils->Creator->TimeToStr(server_uptime).c_str());
	}
}
