/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
#include "wildcard.h"
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

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

static std::map<std::string, std::string> warned;       /* Server names that have had protocol violation warnings displayed for them */

int TreeSocket::WriteLine(std::string line)
{
	Instance->Log(DEBUG, "S[%d] O %s", this->GetFd(), line.c_str());
	line.append("\r\n");
	return this->Write(line);
}


/* Handle ERROR command */
bool TreeSocket::Error(std::deque<std::string> &params)
{
	if (params.size() < 1)
		return false;
	this->Instance->SNO->WriteToSnoMask('l',"ERROR from %s: %s",(!InboundServerName.empty() ? InboundServerName.c_str() : myhost.c_str()),params[0].c_str());
	/* we will return false to cause the socket to close. */
	return false;
}

/** TODO: This creates a total mess of output and needs to really use irc::modestacker.
 */
bool TreeSocket::RemoveStatus(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	Channel* c = Instance->FindChan(params[0]);
	if (c)
	{
		for (char modeletter = 'A'; modeletter <= 'z'; modeletter++)
		{
			ModeHandler* mh = Instance->Modes->FindMode(modeletter, MODETYPE_CHANNEL);
			if (mh)
				mh->RemoveMode(c);
		}
	}
	return true;
}

bool TreeSocket::RemoteServer(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 5)
	{
		SendError("Protocol error - Missing SID");
		return false;
	}

	std::string servername = params[0];
	std::string password = params[1];
	// hopcount is not used for a remote server, we calculate this ourselves
	std::string sid = params[3];
	std::string description = params[4];
	TreeServer* ParentOfThis = Utils->FindServer(prefix);
	if (!ParentOfThis)
	{
		this->SendError("Protocol error - Introduced remote server from unknown server "+prefix);
		return false;
	}
	if (!this->Instance->IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return false;
	}
	TreeServer* CheckDupe = Utils->FindServer(servername);
	if (CheckDupe)
	{
		this->SendError("Server "+servername+" already exists!");
		this->Instance->SNO->WriteToSnoMask('l',"Server \2"+servername+"\2 being introduced from \2" + prefix + "\2 denied, already exists. Closing link with " + prefix);
		return false;
	}

	Link* lnk = Utils->FindLink(servername);

	TreeServer *Node = new TreeServer(this->Utils, this->Instance, servername, description, sid, ParentOfThis,NULL, lnk ? lnk->Hidden : false);

	if (Node->DuplicateID())
	{
		this->SendError("Server ID "+sid+" already exists on the network!");
		this->Instance->SNO->WriteToSnoMask('l',"Server \2"+servername+"\2 being introduced from \2" + prefix + "\2 denied, server ID already exists on the network. Closing link with " + prefix);
		return false;
	}

	ParentOfThis->AddChild(Node);
	params[4] = ":" + params[4];
	Utils->DoOneToAllButSender(prefix,"SERVER",params,prefix);
	this->Instance->SNO->WriteToSnoMask('l',"Server \002"+prefix+"\002 introduced server \002"+servername+"\002 ("+description+")");
	return true;
}

bool TreeSocket::ComparePass(const std::string &ours, const std::string &theirs)
{
	if ((!strncmp(ours.c_str(), "HMAC-SHA256:", 12)) || (!strncmp(theirs.c_str(), "HMAC-SHA256:", 12)))
	{
		/* One or both of us specified hmac sha256, but we don't have sha256 module loaded!
		 * We can't allow this password as valid.
		 */
		if (!Instance->Modules->Find("m_sha256.so") || !Utils->ChallengeResponse)
				return false;
		else
			/* Straight string compare of hashes */
			return ours == theirs;
	}
	else
		/* Straight string compare of plaintext */
		return ours == theirs;
}

bool TreeSocket::Outbound_Reply_Server(std::deque<std::string> &params)
{
	if (params.size() < 5)
	{
		SendError("Protocol error - Missing SID");
		return false;
	}

	irc::string servername = params[0].c_str();
	std::string sname = params[0];
	std::string password = params[1];
	std::string sid = params[3];
	std::string description = params[4];
	int hops = atoi(params[2].c_str());

	this->InboundServerName = sname;
	this->InboundDescription = description;
	this->InboundSID = sid;

	if (!sentcapab)
		this->SendCapabilities();

	if (hops)
	{
		this->SendError("Server too far away for authentication");
		this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server is too far away for authentication");
		return false;
	}

	if (!this->Instance->IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return false;
	}

	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
		if ((x->Name == servername) && ((ComparePass(this->MakePass(x->RecvPass,this->GetOurChallenge()),password)) || (x->RecvPass == password && (this->GetTheirChallenge().empty()))))
		{
			TreeServer* CheckDupe = Utils->FindServer(sname);
			if (CheckDupe)
			{
				this->SendError("Server "+sname+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
				this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists on server "+CheckDupe->GetParent()->GetName());
				return false;
			}
			// Begin the sync here. this kickstarts the
			// other side, waiting in WAIT_AUTH_2 state,
			// into starting their burst, as it shows
			// that we're happy.
			this->LinkState = CONNECTED;
			// we should add the details of this server now
			// to the servers tree, as a child of the root
			// node.

			TreeServer *Node = new TreeServer(this->Utils, this->Instance, sname, description, sid, Utils->TreeRoot, this, x->Hidden);

			if (Node->DuplicateID())
			{
				this->SendError("Server ID "+sid+" already exists on the network!");
				this->Instance->SNO->WriteToSnoMask('l',"Server \2"+assign(servername)+"\2 being introduced denied, server ID already exists on the network. Closing link.");
				return false;
			}

			Utils->TreeRoot->AddChild(Node);
			params[4] = ":" + params[4];
			Utils->DoOneToAllButSender(Instance->Config->GetSID(),"SERVER",params,sname);
			Node->bursting = true;
			this->DoBurst(Node);
			return true;
		}
	}
	this->SendError("Invalid credentials");
	this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
	return false;
}

bool TreeSocket::Inbound_Server(std::deque<std::string> &params)
{
	if (params.size() < 5)
	{
		SendError("Protocol error - Missing SID");
		return false;
	}

	irc::string servername = params[0].c_str();
	std::string sname = params[0];
	std::string password = params[1];
	std::string sid = params[3];
	std::string description = params[4];
	int hops = atoi(params[2].c_str());

	this->InboundServerName = sname;
	this->InboundDescription = description;
	this->InboundSID = sid;

	if (!sentcapab)
		this->SendCapabilities();

	if (hops)
	{
		this->SendError("Server too far away for authentication");
		this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server is too far away for authentication");
		return false;
	}

	if (!this->Instance->IsSID(sid))
	{
		this->SendError("Invalid format server ID: "+sid+"!");
		return false;
	}

	for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
	{
		if ((x->Name == servername) && ((ComparePass(this->MakePass(x->RecvPass,this->GetOurChallenge()),password) || x->RecvPass == password && (this->GetTheirChallenge().empty()))))
		{
			/* Check for fully initialized instances of the server by id */
			Instance->Log(DEBUG,"Looking for dupe SID %s", sid.c_str());
			TreeServer* CheckDupeSID = Utils->FindServerID(sid);
			if (CheckDupeSID)
			{
				this->SendError("Server ID "+CheckDupeSID->GetID()+" already exists on server "+CheckDupeSID->GetName()+"!");
				this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, server ID '"+CheckDupeSID->GetID()+
						"' already exists on server "+CheckDupeSID->GetName());
				return false;
			}
			/* Now check for fully initialized instances of the server by name */
			TreeServer* CheckDupe = Utils->FindServer(sname);
			if (CheckDupe)
			{
				this->SendError("Server "+sname+" already exists on server "+CheckDupe->GetParent()->GetName()+"!");
				this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, already exists on server "+CheckDupe->GetParent()->GetName());
				return false;
			}
			this->Instance->SNO->WriteToSnoMask('l',"Verified incoming server connection from \002"+sname+"\002["+(x->HiddenFromStats ? "<hidden>" : this->GetIP())+"] ("+description+")");
			if (this->Hook)
			{
				std::string name = BufferedSocketNameRequest((Module*)Utils->Creator, this->Hook).Send();
				this->Instance->SNO->WriteToSnoMask('l',"Connection from \2"+sname+"\2["+(x->HiddenFromStats ? "<hidden>" : this->GetIP())+"] using transport \2"+name+"\2");
			}

			// this is good. Send our details: Our server name and description and hopcount of 0,
			// along with the sendpass from this block.
			this->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+this->MakePass(x->SendPass, this->GetTheirChallenge())+" 0 "+Instance->Config->GetSID()+" :"+this->Instance->Config->ServerDesc);
			// move to the next state, we are now waiting for THEM.
			this->LinkState = WAIT_AUTH_2;
			return true;
		}
	}
	this->SendError("Invalid credentials");
	this->Instance->SNO->WriteToSnoMask('l',"Server connection from \2"+sname+"\2 denied, invalid link credentials");
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

	Instance->Log(DEBUG, "S[%d] I %s", this->GetFd(), line.c_str());

	this->Split(line.c_str(),params);
	
	if (params.empty())
		return true;
	
	if ((params[0][0] == ':') && (params.size() > 1))
	{
		prefix = params[0].substr(1);
		params.pop_front();
		
		if (prefix.empty())
		{
			this->SendError("BUG (?) Empty prefix recieved.");
			return false;
		}
	}
	
	command = params[0].c_str();
	params.pop_front();

	switch (this->LinkState)
	{
		TreeServer* Node;

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
				if (params.size() && Utils->EnableTimeSync)
				{
					bool we_have_delta = (Instance->Time(false) != Instance->Time(true));
					time_t them = atoi(params[0].c_str());
					time_t delta = them - Instance->Time(false);
					if ((delta < -600) || (delta > 600))
					{
						Instance->SNO->WriteToSnoMask('l',"\2ERROR\2: Your clocks are out by %d seconds (this is more than five minutes). Link aborted, \2PLEASE SYNC YOUR CLOCKS!\2",abs(delta));
						SendError("Your clocks are out by "+ConvToStr(abs(delta))+" seconds (this is more than five minutes). Link aborted, PLEASE SYNC YOUR CLOCKS!");
						return false;
					}
					else if ((delta < -30) || (delta > 30))
					{
						Instance->SNO->WriteToSnoMask('l',"\2WARNING\2: Your clocks are out by %d seconds. Please consider synching your clocks.", abs(delta));
					}

					if (!Utils->MasterTime && !we_have_delta)
					{
						this->Instance->SetTimeDelta(delta);
						// Send this new timestamp to any other servers
						Utils->DoOneToMany(Instance->Config->GetSID(), "TIMESET", params);
					}
				}
				this->LinkState = CONNECTED;
				Link* lnk = Utils->FindLink(InboundServerName);

				Node = new TreeServer(this->Utils, this->Instance, InboundServerName, InboundDescription, InboundSID, Utils->TreeRoot, this, lnk ? lnk->Hidden : false);

				if (Node->DuplicateID())
				{
					this->SendError("Server ID "+InboundSID+" already exists on the network!");
					this->Instance->SNO->WriteToSnoMask('l',"Server \2"+InboundServerName+"\2 being introduced from \2" + prefix + "\2 denied, server ID already exists on the network. Closing link.");
					return false;
				}

				Utils->TreeRoot->AddChild(Node);
				params.clear();
				params.push_back(InboundServerName);
				params.push_back("*");
				params.push_back("1");
				params.push_back(InboundSID);
				params.push_back(":"+InboundDescription);
				Utils->DoOneToAllButSender(Instance->Config->GetSID(),"SERVER",params,InboundServerName);
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
		case LISTENER:
			/*
			 * This really shouldn't happen.
			 */
			this->SendError("Internal error -- listening socket accepted its own descriptor!!!");
			return false;
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
				 */
				std::string direction = prefix;

				User *t = this->Instance->FindUUID(prefix);
				if (t)
				{
					direction = t->server;
				}

				TreeServer* route_back_again = Utils->BestRouteTo(direction);
				if ((!route_back_again) || (route_back_again->GetSocket() != this))
				{
					if (route_back_again)
						Instance->Log(DEBUG,"Protocol violation: Fake direction in command '%s' from connection '%s'",line.c_str(),this->GetName().c_str());
					return true;
				}
				/* Fix by brain:
				 * When there is activity on the socket, reset the ping counter so
				 * that we're not wasting bandwidth pinging an active server.
				 */
				route_back_again->SetNextPingTime(time(NULL) + Utils->PingFreq);
				route_back_again->SetPingFlag();
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
			if (command == "MODE")
			{
				if (params.size() >= 2)
				{
					Channel* channel = Instance->FindChan(params[0]);
					if (channel)
					{
						User* x = Instance->FindNick(prefix);
						if (x)
						{
							if (warned.find(x->server) == warned.end())
							{
								Instance->Log(DEFAULT,"WARNING: I revceived modes '%s' from another server '%s'. This is not compliant with InspIRCd. Please check that server for bugs.", params[1].c_str(), x->server);
								Instance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending nonstandard modes: '%s MODE %s' where FMODE should be used, and may cause desyncs.", x->server, x->nick, params[1].c_str());
								warned[x->server] = x->nick;
							}
						}
					}
				}
			}
			else if (command == "SVSMODE")
			{
				command = "MODE";
			}


			/*
			 * Now, check for (and parse) commands as appropriate. -- w
			 */	
		
			/* Find the server that this command originated from, used in the handlers below */
			TreeServer *ServerSource = Utils->FindServer(prefix);

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
			else if (command == "REHASH")
			{
				return this->RemoteRehash(prefix,params);
			}
			else if (command == "METADATA")
			{
				return this->MetaData(prefix,params);
			}
			else if (command == "REMSTATUS")
			{
				return this->RemoveStatus(prefix,params);
			}
			else if (command == "PING")
			{
				return this->LocalPing(prefix,params);
			}
			else if (command == "PONG")
			{
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
			else if (command == "TIMESET")
			{
				return this->HandleSetTime(prefix, params);
			}
			else if (command == "TIME")
			{
				return this->Time(prefix,params);
			}
			else if ((command == "KICK") && (Utils->IsServer(prefix)))
			{
				if (params.size() == 3)
				{
					User* user = this->Instance->FindNick(params[1]);
					Channel* chan = this->Instance->FindChan(params[0]);
					if (user && chan)
					{
						if (!chan->ServerKickUser(user, params[2].c_str(), false))
							/* Yikes, the channels gone! */
							delete chan;
					}
				}

				return Utils->DoOneToAllButSenderRaw(line,sourceserv,prefix,command,params);
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
			else if (command == "OPERNOTICE")
			{
				if (params.size() >= 1)
					Instance->SNO->WriteToSnoMask('A', "From " + prefix + ": " + params[0]);
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "MODENOTICE")
			{
				if (params.size() >= 2)
				{
					Instance->Users->WriteMode(params[0].c_str(), WM_AND, "*** From %s: %s", prefix.c_str(), params[1].c_str());
				}
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "SNONOTICE")
			{
				if (params.size() >= 2)
				{
					Instance->SNO->WriteToSnoMask(*(params[0].c_str()), "From " + prefix + ": "+ params[1]);
				}
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "BURST")
			{
				// Set prefix server as bursting
				if (!ServerSource)
				{
					this->Instance->SNO->WriteToSnoMask('l', "WTF: Got BURST from a nonexistant server(?): %s", prefix.c_str());
					return false;
				}
				
				ServerSource->bursting = true;
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "ENDBURST")
			{
				if (!ServerSource)
				{
					this->Instance->SNO->WriteToSnoMask('l', "WTF: Got ENDBURST from a nonexistant server(?): %s", prefix.c_str());
					return false;
				}
				
				ServerSource->FinishBurst();
				return Utils->DoOneToAllButSenderRaw(line, sourceserv, prefix, command, params);
			}
			else if (command == "MODE")
			{
				// Server-prefix MODE.
				const char* modelist[MAXPARAMETERS];
				for (size_t i = 0; i < params.size(); i++)
					modelist[i] = params[i].c_str();
					
				// Insert into the parser
				this->Instance->SendMode(modelist, params.size(), this->Instance->FakeClient);
				
				// Pass out to the network
				return Utils->DoOneToAllButSenderRaw(line,sourceserv,prefix,command,params);
			}
			else
			{
				/*
				 * Not a special s2s command. Emulate the user doing it.
				 * This saves us having a huge ugly command parser again.
				 */
				User *who = this->Instance->FindUUID(prefix);

				if (!who)
				{
					// this looks ugly because command is an irc::string
					this->SendError("Command (" + std::string(command.c_str()) + ") from unknown prefix (" + prefix + ")! Dropping link.");
					return false;
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
					User* x = this->Instance->FindNickOnly(params[0]);
					if ((x) && (x != who))
					{
						int collideret = 0;
						/* x is local, who is remote */
						collideret = this->DoCollision(x, who->age, who->ident, who->GetIPString(), who->uuid);
						if (collideret != 1)
						{
							/*
							 * Remote client lost, or both lost, parsing this nickchange would be
							 * pointless, as the incoming client's server will soon recieve SVSNICK to
							 * change its nick to its UID. :) -- w00t
							 */
							return true;
						}
					}
				}
					
				// its a user
				const char* strparams[127];
				for (unsigned int q = 0; q < params.size(); q++)
				{
					strparams[q] = params[q].c_str();
				}

				switch (this->Instance->CallCommandHandler(command.c_str(), strparams, params.size(), who))
				{
					case CMD_INVALID:
						// command is irc::string, hence ugliness
						this->SendError("Unrecognised or malformed command '" + std::string(command.c_str()) + "' -- possibly loaded mismatched modules");
						return false;
						break;
					/*
					 * CMD_LOCALONLY is aliased to CMD_FAILURE, so this won't go out onto the network.
					 */
					case CMD_FAILURE:
						return true;
						break;
					default:
						/* CMD_SUCCESS and CMD_USER_DELETED fall through here */
						break;
				}

				return Utils->DoOneToAllButSenderRaw(line,sourceserv,prefix,command,params);

			}
			return true;
			break; // end of state CONNECTED (phew).
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
		Utils->Creator->RemoteMessage(NULL, "CONNECT: Connection to \002%s\002 timed out.", myhost.c_str());
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
		Utils->Creator->RemoteMessage(NULL,"Connection to '\2%s\2' failed.",quitserver.c_str());
		time_t server_uptime = Instance->Time() - this->age;	
		if (server_uptime)
			Utils->Creator->RemoteMessage(NULL,"Connection to '\2%s\2' was established for %s", quitserver.c_str(), Utils->Creator->TimeToStr(server_uptime).c_str());
	}
}

int TreeSocket::OnIncomingConnection(int newsock, char* ip)
{
	/* To prevent anyone from attempting to flood opers/DDoS by connecting to the server port,
	 * or discovering if this port is the server port, we don't allow connections from any
	 * IPs for which we don't have a link block.
	 */
	bool found = false;

	found = (std::find(Utils->ValidIPs.begin(), Utils->ValidIPs.end(), ip) != Utils->ValidIPs.end());
	if (!found)
	{
		for (std::vector<std::string>::iterator i = Utils->ValidIPs.begin(); i != Utils->ValidIPs.end(); i++)
			if (irc::sockets::MatchCIDR(ip, (*i).c_str()))
				found = true;

		if (!found)
		{
			Utils->Creator->RemoteMessage(NULL,"Server connection from %s denied (no link blocks with that IP address)", ip);
			Instance->SE->Close(newsock);
			return false;
		}
	}

	TreeSocket* s = new TreeSocket(this->Utils, this->Instance, newsock, ip, this->Hook);
	s = s; /* Whinge whinge whinge, thats all GCC ever does. */
	return true;
}
