/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
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
#include "m_hash.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_hash.h */


/** Because most of the I/O gubbins are encapsulated within
 * BufferedSocket, we just call the superclass constructor for
 * most of the action, and append a few of our own values
 * to it.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, Module* HookMod)
	: BufferedSocket(SI, host, port, listening, maxtime), Utils(Util), Hook(HookMod)
{
	myhost = host;
	this->LinkState = LISTENER;
	theirchallenge.clear();
	ourchallenge.clear();
	if (listening && Hook)
		BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
}

TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, const std::string &ServerName, const std::string &bindto, Module* HookMod)
	: BufferedSocket(SI, host, port, listening, maxtime, bindto), Utils(Util), Hook(HookMod)
{
	myhost = ServerName;
	theirchallenge.clear();
	ourchallenge.clear();
	this->LinkState = CONNECTING;
	if (Hook)
		BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
}

/** When a listening socket gives us a new file descriptor,
 * we must associate it with a socket without creating a new
 * connection. This constructor is used for this purpose.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, int newfd, char* ip, Module* HookMod)
	: BufferedSocket(SI, newfd, ip), Utils(Util), Hook(HookMod)
{
	this->LinkState = WAIT_AUTH_1;
	theirchallenge.clear();
	ourchallenge.clear();
	sentcapab = false;
	/* If we have a transport module hooked to the parent, hook the same module to this
	 * socket, and set a timer waiting for handshake before we send CAPAB etc.
	 */
	if (Hook)
		BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();

	Instance->Timers->AddTimer(new HandshakeTimer(Instance, this, &(Utils->LinkBlocks[0]), this->Utils, 1));
}

ServerState TreeSocket::GetLinkState()
{
	return this->LinkState;
}

Module* TreeSocket::GetHook()
{
	return this->Hook;
}

TreeSocket::~TreeSocket()
{
	if (Hook)
		BufferedSocketUnhookRequest(this, (Module*)Utils->Creator, Hook).Send();

	Utils->DelBurstingServer(this);
}

const std::string& TreeSocket::GetOurChallenge()
{
	return this->ourchallenge;
}

void TreeSocket::SetOurChallenge(const std::string &c)
{
	this->ourchallenge = c;
}

const std::string& TreeSocket::GetTheirChallenge()
{
	return this->theirchallenge;
}

void TreeSocket::SetTheirChallenge(const std::string &c)
{
	this->theirchallenge = c;
}

std::string TreeSocket::MakePass(const std::string &password, const std::string &challenge)
{
	/* This is a simple (maybe a bit hacky?) HMAC algorithm, thanks to jilles for
	 * suggesting the use of HMAC to secure the password against various attacks.
	 *
	 * Note: If m_sha256.so is not loaded, we MUST fall back to plaintext with no
	 *       HMAC challenge/response.
	 */
	Module* sha256 = Instance->Modules->Find("m_sha256.so");
	if (Utils->ChallengeResponse && sha256 && !challenge.empty())
	{
		/* XXX: This is how HMAC is supposed to be done:
		 *
		 * sha256( (pass xor 0x5c) + sha256((pass xor 0x36) + m) )
		 *
		 * Note that we are encoding the hex hash, not the binary
		 * output of the hash which is slightly different to standard.
		 *
		 * Don't ask me why its always 0x5c and 0x36... it just is.
		 */
		std::string hmac1, hmac2;

		for (size_t n = 0; n < password.length(); n++)
		{
			hmac1 += static_cast<char>(password[n] ^ 0x5C);
			hmac2 += static_cast<char>(password[n] ^ 0x36);
		}

		hmac2 += challenge;
		HashResetRequest(Utils->Creator, sha256).Send();
		hmac2 = HashSumRequest(Utils->Creator, sha256, hmac2).Send();

		HashResetRequest(Utils->Creator, sha256).Send();
		std::string hmac = hmac1 + hmac2;
		hmac = HashSumRequest(Utils->Creator, sha256, hmac).Send();

		return "HMAC-SHA256:"+ hmac;
	}
	else if (!challenge.empty() && !sha256)
		Instance->Log(DEFAULT,"Not authenticating to server using SHA256/HMAC because we don't have m_sha256 loaded!");

	return password;
}

/** When an outbound connection finishes connecting, we receive
 * this event, and must send our SERVER string to the other
 * side. If the other side is happy, as outlined in the server
 * to server docs on the inspircd.org site, the other side
 * will then send back its own server string.
 */
bool TreeSocket::OnConnected()
{
	if (this->LinkState == CONNECTING)
	{
		/* we do not need to change state here. */
		for (std::vector<Link>::iterator x = Utils->LinkBlocks.begin(); x < Utils->LinkBlocks.end(); x++)
		{
			if (x->Name == this->myhost)
			{
				Utils->Creator->RemoteMessage(NULL,"Connection to \2%s\2[%s] started.", myhost.c_str(), (x->HiddenFromStats ? "<hidden>" : this->GetIP().c_str()));
				if (Hook)
				{
					BufferedSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
					Utils->Creator->RemoteMessage(NULL,"Connection to \2%s\2[%s] using transport \2%s\2", myhost.c_str(), (x->HiddenFromStats ? "<hidden>" : this->GetIP().c_str()),
							x->Hook.c_str());
				}
				this->OutboundPass = x->SendPass;
				sentcapab = false;

				/* found who we're supposed to be connecting to, send the neccessary gubbins. */
				if (this->GetHook())
					Instance->Timers->AddTimer(new HandshakeTimer(Instance, this, &(*x), this->Utils, 1));
				else
					this->SendCapabilities();

				return true;
			}
		}
	}
	/* There is a (remote) chance that between the /CONNECT and the connection
	 * being accepted, some muppet has removed the <link> block and rehashed.
	 * If that happens the connection hangs here until it's closed. Unlikely
	 * and rather harmless.
	 */
	this->Utils->Creator->RemoteMessage(NULL,"Connection to \2%s\2 lost link tag(!)", myhost.c_str());
	return true;
}

void TreeSocket::OnError(BufferedSocketError e)
{
	Link* MyLink;

	if (this->LinkState == LISTENER)
		return;

	switch (e)
	{
		case I_ERR_CONNECT:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: Connection to \002%s\002 refused", myhost.c_str());
			MyLink = Utils->FindLink(myhost);
			if (MyLink)
				Utils->DoFailOver(MyLink);
		break;
		case I_ERR_SOCKET:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: Could not create socket");
		break;
		case I_ERR_BIND:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: Error binding socket to address or port");
		break;
		case I_ERR_WRITE:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: I/O error on connection");
		break;
		case I_ERR_NOMOREFDS:
			Utils->Creator->RemoteMessage(NULL,"Connection failed: Operating system is out of file descriptors!");
		break;
		default:
			if ((errno) && (errno != EINPROGRESS) && (errno != EAGAIN))
				Utils->Creator->RemoteMessage(NULL,"Connection to \002%s\002 failed with OS error: %s", myhost.c_str(), strerror(errno));
		break;
	}
}

int TreeSocket::OnDisconnect()
{
	/* For the same reason as above, we don't
	 * handle OnDisconnect()
	 */
	return true;
}

/** Recursively send the server tree with distances as hops.
 * This is used during network burst to inform the other server
 * (and any of ITS servers too) of what servers we know about.
 * If at any point any of these servers already exist on the other
 * end, our connection may be terminated. The hopcounts given
 * by this function are relative, this doesn't matter so long as
 * they are all >1, as all the remote servers re-calculate them
 * to be relative too, with themselves as hop 0.
 */
void TreeSocket::SendServers(TreeServer* Current, TreeServer* s, int hops)
{
	char command[1024];
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		TreeServer* recursive_server = Current->GetChild(q);
		if (recursive_server != s)
		{
			snprintf(command,1024,":%s SERVER %s * %d %s :%s",Current->GetName().c_str(),recursive_server->GetName().c_str(),hops,
					recursive_server->GetID().c_str(),
					recursive_server->GetDesc().c_str());
			this->WriteLine(command);
			this->WriteLine(":"+recursive_server->GetName()+" VERSION :"+recursive_server->GetVersion());
			/* down to next level */
			this->SendServers(recursive_server, s, hops+1);
		}
	}
}

std::string TreeSocket::MyCapabilities()
{
	std::vector<std::string> modlist = this->Instance->Modules->GetAllModuleNames(VF_COMMON);
	std::string capabilities;
	sort(modlist.begin(),modlist.end());
	for (unsigned int i = 0; i < modlist.size(); i++)
	{
		if (i)
			capabilities = capabilities + ",";
		capabilities = capabilities + modlist[i];
	}
	return capabilities;
}

std::string TreeSocket::RandString(unsigned int length)
{
	char* randombuf = new char[length+1];
	std::string out;
#ifdef WINDOWS
	int fd = -1;
#else
	int fd = open("/dev/urandom", O_RDONLY, 0);
#endif

	if (fd >= 0)
	{
#ifndef WINDOWS
		read(fd, randombuf, length);
		close(fd);
#endif
	}
	else
	{
		for (unsigned int i = 0; i < length; i++)
			randombuf[i] = rand();
	}

	for (unsigned int i = 0; i < length; i++)
	{
		char randchar = static_cast<char>((randombuf[i] & 0x7F) | 0x21);
		out += (randchar == '=' ? '_' : randchar);
	}

	delete[] randombuf;
	return out;
}

void TreeSocket::SendCapabilities()
{
	if (sentcapab)
		return;

	sentcapab = true;
	irc::commasepstream modulelist(MyCapabilities());
	this->WriteLine("CAPAB START");

	/* Send module names, split at 509 length */
	std::string item;
	std::string line = "CAPAB MODULES ";
	while (modulelist.GetToken(item))
	{
		if (line.length() + item.length() + 1 > 509)
		{
			this->WriteLine(line);
			line = "CAPAB MODULES ";
		}

		if (line != "CAPAB MODULES ")
			line.append(",");

		line.append(item);
	}
	if (line != "CAPAB MODULES ")
		this->WriteLine(line);

	int ip6 = 0;
	int ip6support = 0;
#ifdef IPV6
	ip6 = 1;
#endif
#ifdef SUPPORT_IP6LINKS
	ip6support = 1;
#endif
	std::string extra;
	/* Do we have sha256 available? If so, we send a challenge */
	if (Utils->ChallengeResponse && (Instance->Modules->Find("m_sha256.so")))
	{
		this->SetOurChallenge(RandString(20));
		extra = " CHALLENGE=" + this->GetOurChallenge();
	}

	this->WriteLine("CAPAB CAPABILITIES :NICKMAX="+ConvToStr(NICKMAX)+" HALFOP="+ConvToStr(this->Instance->Config->AllowHalfop)+" CHANMAX="+ConvToStr(CHANMAX)+" MAXMODES="+ConvToStr(MAXMODES)+" IDENTMAX="+ConvToStr(IDENTMAX)+" MAXQUIT="+ConvToStr(MAXQUIT)+" MAXTOPIC="+ConvToStr(MAXTOPIC)+" MAXKICK="+ConvToStr(MAXKICK)+" MAXGECOS="+ConvToStr(MAXGECOS)+" MAXAWAY="+ConvToStr(MAXAWAY)+" IP6NATIVE="+ConvToStr(ip6)+" IP6SUPPORT="+ConvToStr(ip6support)+" PROTOCOL="+ConvToStr(ProtocolVersion)+extra+" PREFIX="+Instance->Modes->BuildPrefixes()+" CHANMODES="+Instance->Modes->ChanModes()+" SVSPART=1");

	this->WriteLine("CAPAB END");
}

/* Check a comma seperated list for an item */
bool TreeSocket::HasItem(const std::string &list, const std::string &item)
{
	irc::commasepstream seplist(list);
	std::string item2;

	while (seplist.GetToken(item2))
	{
		if (item2 == item)
			return true;
	}
	return false;
}

/* Isolate and return the elements that are different between two comma seperated lists */
std::string TreeSocket::ListDifference(const std::string &one, const std::string &two)
{
	irc::commasepstream list_one(one);
	std::string item;
	std::string result;
	while (list_one.GetToken(item))
	{
		if (!HasItem(two, item))
		{
			result.append(" ");
			result.append(item);
		}
	}
	return result;
}

void TreeSocket::SendError(const std::string &errormessage)
{
	/* Display the error locally as well as sending it remotely */
	Utils->Creator->RemoteMessage(NULL, "Sent \2ERROR\2 to %s: %s", (this->InboundServerName.empty() ? "<unknown>" : this->InboundServerName.c_str()), errormessage.c_str());
	this->WriteLine("ERROR :"+errormessage);
	/* One last attempt to make sure the error reaches its target */
	this->FlushWriteBuffer();
}

bool TreeSocket::Capab(const std::deque<std::string> &params)
{
	if (params.size() < 1)
	{
		this->SendError("Invalid number of parameters for CAPAB - Mismatched version");
		return false;
	}
	if (params[0] == "START")
	{
		this->ModuleList.clear();
		this->CapKeys.clear();
	}
	else if (params[0] == "END")
	{
		std::string reason;
		int ip6support = 0;
#ifdef SUPPORT_IP6LINKS
		ip6support = 1;
#endif
		/* Compare ModuleList and check CapKeys...
		 * Maybe this could be tidier? -- Brain
		 */
		if ((this->ModuleList != this->MyCapabilities()) && (this->ModuleList.length()))
		{
			std::string diff = ListDifference(this->ModuleList, this->MyCapabilities());
			if (!diff.length())
			{
				diff = "your server:" + ListDifference(this->MyCapabilities(), this->ModuleList);
			}
			else
			{
				diff = "this server:" + diff;
			}
			if (diff.length() == 12)
				reason = "Module list in CAPAB is not alphabetically ordered, cannot compare lists.";
			else
				reason = "Modules loaded on these servers are not correctly matched, these modules are not loaded on " + diff;
		}

		cap_validation valid_capab[] = { 
			{"Maximum nickname lengths differ or remote nickname length not specified", "NICKMAX", NICKMAX},
			{"Maximum ident lengths differ or remote ident length not specified", "IDENTMAX", IDENTMAX},
			{"Maximum channel lengths differ or remote channel length not specified", "CHANMAX", CHANMAX},
			{"Maximum modes per line differ or remote modes per line not specified", "MAXMODES", MAXMODES},
			{"Maximum quit lengths differ or remote quit length not specified", "MAXQUIT", MAXQUIT},
			{"Maximum topic lengths differ or remote topic length not specified", "MAXTOPIC", MAXTOPIC},
			{"Maximum kick lengths differ or remote kick length not specified", "MAXKICK", MAXKICK},
			{"Maximum GECOS (fullname) lengths differ or remote GECOS length not specified", "MAXGECOS", MAXGECOS},
			{"Maximum awaymessage lengths differ or remote awaymessage length not specified", "MAXAWAY", MAXAWAY},
			{"", "", 0}
		};

		if (((this->CapKeys.find("IP6SUPPORT") == this->CapKeys.end()) && (ip6support)) || ((this->CapKeys.find("IP6SUPPORT") != this->CapKeys.end()) && (this->CapKeys.find("IP6SUPPORT")->second != ConvToStr(ip6support))))
			reason = "We don't both support linking to IPV6 servers";
		if (((this->CapKeys.find("IP6NATIVE") != this->CapKeys.end()) && (this->CapKeys.find("IP6NATIVE")->second == "1")) && (!ip6support))
			reason = "The remote server is IPV6 native, and we don't support linking to IPV6 servers";
		if (((this->CapKeys.find("PROTOCOL") == this->CapKeys.end()) || ((this->CapKeys.find("PROTOCOL") != this->CapKeys.end()) && (this->CapKeys.find("PROTOCOL")->second != ConvToStr(ProtocolVersion)))))
		{
			if (this->CapKeys.find("PROTOCOL") != this->CapKeys.end())
				reason = "Mismatched protocol versions "+this->CapKeys.find("PROTOCOL")->second+" and "+ConvToStr(ProtocolVersion);
			else
				reason = "Protocol version not specified";
		}

		if(this->CapKeys.find("PREFIX") != this->CapKeys.end() && this->CapKeys.find("PREFIX")->second != this->Instance->Modes->BuildPrefixes())
			reason = "One or more of the prefixes on the remote server are invalid on this server.";

		if (((this->CapKeys.find("HALFOP") == this->CapKeys.end()) && (Instance->Config->AllowHalfop)) || ((this->CapKeys.find("HALFOP") != this->CapKeys.end()) && (this->CapKeys.find("HALFOP")->second != ConvToStr(Instance->Config->AllowHalfop))))
			reason = "We don't both have halfop support enabled/disabled identically";

		for (int x = 0; valid_capab[x].size; ++x)
		{
			if (((this->CapKeys.find(valid_capab[x].key) == this->CapKeys.end()) ||	((this->CapKeys.find(valid_capab[x].key) != this->CapKeys.end()) &&
						 (this->CapKeys.find(valid_capab[x].key)->second != ConvToStr(valid_capab[x].size)))))
				reason = valid_capab[x].reason;
		}
	
		/* Challenge response, store their challenge for our password */
		std::map<std::string,std::string>::iterator n = this->CapKeys.find("CHALLENGE");
		if (Utils->ChallengeResponse && (n != this->CapKeys.end()) && (Instance->Modules->Find("m_sha256.so")))
		{
			/* Challenge-response is on now */
			this->SetTheirChallenge(n->second);
			if (!this->GetTheirChallenge().empty() && (this->LinkState == CONNECTING))
			{
				this->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+this->MakePass(OutboundPass, this->GetTheirChallenge())+" 0 "+
						Instance->Config->GetSID()+" :"+this->Instance->Config->ServerDesc);
			}
		}
		else
		{
			/* They didnt specify a challenge or we don't have m_sha256.so, we use plaintext */
			if (this->LinkState == CONNECTING)
				this->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+OutboundPass+" 0 "+Instance->Config->GetSID()+" :"+this->Instance->Config->ServerDesc);
		}

		if (reason.length())
		{
			this->SendError("CAPAB negotiation failed: "+reason);
			return false;
		}
	}
	else if ((params[0] == "MODULES") && (params.size() == 2))
	{
		if (!this->ModuleList.length())
		{
			this->ModuleList.append(params[1]);
		}
		else
		{
			this->ModuleList.append(",");
			this->ModuleList.append(params[1]);
		}
	}

	else if ((params[0] == "CAPABILITIES") && (params.size() == 2))
	{
		irc::tokenstream capabs(params[1]);
		std::string item;
		bool more = true;
		while ((more = capabs.GetToken(item)))
		{
			/* Process each key/value pair */
			std::string::size_type equals = item.rfind('=');
			if (equals != std::string::npos)
			{
				std::string var = item.substr(0, equals);
				std::string value = item.substr(equals+1, item.length());
				CapKeys[var] = value;
			}
		}
	}
	return true;
}

/** This function forces this server to quit, removing this server
 * and any users on it (and servers and users below that, etc etc).
 * It's very slow and pretty clunky, but luckily unless your network
 * is having a REAL bad hair day, this function shouldnt be called
 * too many times a month ;-)
 */
void TreeSocket::SquitServer(std::string &from, TreeServer* Current)
{
	/* recursively squit the servers attached to 'Current'.
	 * We're going backwards so we don't remove users
	 * while we still need them ;)
	 */
	for (unsigned int q = 0; q < Current->ChildCount(); q++)
	{
		TreeServer* recursive_server = Current->GetChild(q);
		this->SquitServer(from,recursive_server);
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
	if ((Current) && (Current != Utils->TreeRoot))
	{
		Event rmode((char*)Current->GetName().c_str(), (Module*)Utils->Creator, "lost_server");
		rmode.Send(Instance);

		std::deque<std::string> params;
		params.push_back(Current->GetName());
		params.push_back(":"+reason);
		Utils->DoOneToAllButSender(Current->GetParent()->GetName(),"SQUIT",params,Current->GetName());
		if (Current->GetParent() == Utils->TreeRoot)
		{
			this->Instance->SNO->WriteToSnoMask('l',"Server \002"+Current->GetName()+"\002 split: "+reason);
		}
		else
		{
			this->Instance->SNO->WriteToSnoMask('l',"Server \002"+Current->GetName()+"\002 split from server \002"+Current->GetParent()->GetName()+"\002 with reason: "+reason);
		}
		num_lost_servers = 0;
		num_lost_users = 0;
		std::string from = Current->GetParent()->GetName()+" "+Current->GetName();
		SquitServer(from, Current);
		Current->Tidy();
		Current->GetParent()->DelChild(Current);
		delete Current;
		this->Instance->SNO->WriteToSnoMask('l',"Netsplit complete, lost \002%d\002 users on \002%d\002 servers.", num_lost_users, num_lost_servers);
	}
	else
		Instance->Log(DEFAULT,"Squit from unknown server");
}

/** FMODE command - server mode with timestamp checks */
bool TreeSocket::ForceMode(const std::string &source, std::deque<std::string> &params)
{
	/* Chances are this is a 1.0 FMODE without TS */
	if (params.size() < 3)
	{
		/* No modes were in the command, probably a channel with no modes set on it */
		return true;
	}

	bool smode = false;
	std::string sourceserv;
	/* Are we dealing with an FMODE from a user, or from a server? */
	User* who = this->Instance->FindNick(source);
	if (who)
	{
		/* FMODE from a user, set sourceserv to the users server name */
		sourceserv = who->server;
	}
	else
	{
		/* FMODE from a server, use a fake user to receive mode feedback */
		who = this->Instance->FakeClient;
		smode = true;	   /* Setting this flag tells us we should free the User later */
		sourceserv = source;    /* Set sourceserv to the actual source string */
	}
	const char* modelist[64];
	time_t TS = 0;
	int n = 0;
	memset(&modelist,0,sizeof(modelist));
	for (unsigned int q = 0; (q < params.size()) && (q < 64); q++)
	{
		if (q == 1)
		{
			/* The timestamp is in this position.
			 * We don't want to pass that up to the
			 * server->client protocol!
			 */
			TS = atoi(params[q].c_str());
		}
		else
		{
			/* Everything else is fine to append to the modelist */
			modelist[n++] = params[q].c_str();
		}

	}
	/* Extract the TS value of the object, either User or Channel */
	User* dst = this->Instance->FindNick(params[0]);
	Channel* chan = NULL;
	time_t ourTS = 0;
	if (dst)
	{
		ourTS = dst->age;
	}
	else
	{
		chan = this->Instance->FindChan(params[0]);
		if (chan)
		{
			ourTS = chan->age;
		}
		else
			/* Oops, channel doesnt exist! */
			return true;
	}

	if (!TS)
	{
		Instance->Log(DEFAULT,"*** BUG? *** TS of 0 sent to FMODE. Are some services authors smoking craq, or is it 1970 again?. Dropped.");
		Instance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending FMODE with a TS of zero. Total craq. Mode was dropped.", sourceserv.c_str());
		return true;
	}

	/* TS is equal or less: Merge the mode changes into ours and pass on.
	 */
	if (TS <= ourTS)
	{
		if ((TS < ourTS) && (!dst))
			Instance->Log(DEFAULT,"*** BUG *** Channel TS sent in FMODE to %s is %lu which is not equal to %lu!", params[0].c_str(), TS, ourTS);

		if (smode)
		{
			this->Instance->SendMode(modelist, n, who);
		}
		else
		{
			this->Instance->CallCommandHandler("MODE", modelist, n, who);
		}
		/* HOT POTATO! PASS IT ON! */
		Utils->DoOneToAllButSender(source,"FMODE",params,sourceserv);
	}
	/* If the TS is greater than ours, we drop the mode and dont pass it anywhere.
	 */
	return true;
}

/** FTOPIC command */
bool TreeSocket::ForceTopic(const std::string &source, std::deque<std::string> &params)
{
	if (params.size() != 4)
		return true;
	time_t ts = atoi(params[1].c_str());
	std::string nsource = source;
	Channel* c = this->Instance->FindChan(params[0]);
	if (c)
	{
		if ((ts >= c->topicset) || (!*c->topic))
		{
			std::string oldtopic = c->topic;
			strlcpy(c->topic,params[3].c_str(),MAXTOPIC);
			strlcpy(c->setby,params[2].c_str(),127);
			c->topicset = ts;
			/* if the topic text is the same as the current topic,
			 * dont bother to send the TOPIC command out, just silently
			 * update the set time and set nick.
			 */
			if (oldtopic != params[3])
			{
				User* user = this->Instance->FindNick(source);
				if (!user)
				{
					c->WriteChannelWithServ(Instance->Config->ServerName, "TOPIC %s :%s", c->name, c->topic);
				}
				else
				{
					c->WriteChannel(user, "TOPIC %s :%s", c->name, c->topic);
					nsource = user->server;
				}
				/* all done, send it on its way */
				params[3] = ":" + params[3];
				Utils->DoOneToAllButSender(source,"FTOPIC",params,nsource);
			}
		}

	}
	return true;
}

/** FJOIN, similar to TS6 SJOIN, but not quite. */
bool TreeSocket::ForceJoin(const std::string &source, std::deque<std::string> &params)
{
	/* 1.1 FJOIN works as follows:
	 *
	 * Each FJOIN is sent along with a timestamp, and the side with the lowest
	 * timestamp 'wins'. From this point on we will refer to this side as the
	 * winner. The side with the higher timestamp loses, from this point on we
	 * will call this side the loser or losing side. This should be familiar to
	 * anyone who's dealt with dreamforge or TS6 before.
	 *
	 * When two sides of a split heal and this occurs, the following things
	 * will happen:
	 *
	 * If the timestamps are exactly equal, both sides merge their privilages
	 * and users, as in InspIRCd 1.0 and ircd2.8. The channels have not been
	 * re-created during a split, this is safe to do.
	 *
	 * If the timestamps are NOT equal, the losing side removes all of its
	 * modes from the channel, before introducing new users into the channel
	 * which are listed in the FJOIN command's parameters. The losing side then
	 * LOWERS its timestamp value of the channel to match that of the winning
	 * side, and the modes of the users of the winning side are merged in with
	 * the losing side.
	 *
	 * The winning side on the other hand will ignore all user modes from the
	 * losing side, so only its own modes get applied. Life is simple for those
	 * who succeed at internets. :-)
	 *
	 * NOTE: Unlike TS6 and dreamforge and other protocols which have SJOIN,
	 * FJOIN does not contain the simple-modes such as +iklmnsp. Why not,
	 * you ask? Well, quite simply because we don't need to. They'll be sent
	 * after the FJOIN by FMODE, and FMODE is timestamped, so in the event
	 * the losing side sends any modes for the channel which shouldnt win,
	 * they wont as their timestamp will be too high :-)
	 */

	if (params.size() < 3)
		return true;

	irc::modestacker modestack(true);				/* Modes to apply from the users in the user list */
	User* who = NULL;		   				/* User we are currently checking */
	std::string channel = params[0];				/* Channel name, as a string */
	time_t TS = atoi(params[1].c_str());    			/* Timestamp given to us for remote side */
	irc::tokenstream users(params[2]);				/* Users from the user list */
	bool apply_other_sides_modes = true;				/* True if we are accepting the other side's modes */
	Channel* chan = this->Instance->FindChan(channel);		/* The channel we're sending joins to */
	time_t ourTS = chan ? chan->age : Instance->Time(true)+600;	/* The TS of our side of the link */
	bool created = !chan;						/* True if the channel doesnt exist here yet */
	std::string item;						/* One item in the list of nicks */

	params[2] = ":" + params[2];
	Utils->DoOneToAllButSender(source,"FJOIN",params,source);

        if (!TS)
	{
		Instance->Log(DEFAULT,"*** BUG? *** TS of 0 sent to FJOIN. Are some services authors smoking craq, or is it 1970 again?. Dropped.");
		Instance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending FJOIN with a TS of zero. Total craq. Command was dropped.", source.c_str());
		return true;
	}

	/* If our TS is less than theirs, we dont accept their modes */
	if (ourTS < TS)
		apply_other_sides_modes = false;

	/* Our TS greater than theirs, clear all our modes from the channel, accept theirs. */
	if (ourTS > TS)
	{
		std::deque<std::string> param_list;
		if (Utils->AnnounceTSChange && chan)
			chan->WriteChannelWithServ(Instance->Config->ServerName, "NOTICE %s :TS for %s changed from %lu to %lu", chan->name, chan->name, ourTS, TS);
		ourTS = TS;
		if (!created)
		{
			chan->age = TS;
			param_list.push_back(channel);
			this->RemoveStatus(Instance->Config->GetSID(), param_list);
		}
	}

	/* Now, process every 'prefixes,nick' pair */
	while (users.GetToken(item))
	{
		const char* usr = item.c_str();
		if (usr && *usr)
		{
			const char* permissions = usr;
			/* Iterate through all the prefix values, convert them from prefixes to mode letters */
			std::string modes;
			while ((*permissions) && (*permissions != ','))
			{
				ModeHandler* mh = Instance->Modes->FindPrefix(*permissions);
				if (mh)
					modes = modes + mh->GetModeChar();
				else
				{
					this->SendError(std::string("Invalid prefix '")+(*permissions)+"' in FJOIN");
					return false;
				}
				usr++;
				permissions++;
			}
			/* Advance past the comma, to the nick */
			usr++;
			
			/* Check the user actually exists */
			who = this->Instance->FindUUID(usr);
			if (who)
			{
				/* Check that the user's 'direction' is correct */
				TreeServer* route_back_again = Utils->BestRouteTo(who->server);
				if ((!route_back_again) || (route_back_again->GetSocket() != this))
					continue;

				/* Add any permissions this user had to the mode stack */
				for (std::string::iterator x = modes.begin(); x != modes.end(); ++x)
					modestack.Push(*x, who->nick);

				Channel::JoinUser(this->Instance, who, channel.c_str(), true, "", true, TS);
			}
			else
			{
				Instance->Log(SPARSE,"Warning! Invalid user %s in FJOIN to channel %s IGNORED", usr, channel.c_str());
				continue;
			}
		}
	}

	/* Flush mode stacker if we lost the FJOIN or had equal TS */
	if (apply_other_sides_modes)
	{
		std::deque<std::string> stackresult;
		const char* mode_junk[MAXMODES+2];
		mode_junk[0] = channel.c_str();

		while (modestack.GetStackedLine(stackresult))
		{
			for (size_t j = 0; j < stackresult.size(); j++)
			{
				mode_junk[j+1] = stackresult[j].c_str();
			}
			Instance->SendMode(mode_junk, stackresult.size() + 1, Instance->FakeClient);
		}
	}

	return true;
}

/*
 * Yes, this function looks a little ugly.
 * However, in some circumstances we may not have a User, so we need to do things this way.
 * Returns 1 if colliding local client, 2 if colliding remote, 3 if colliding both.
 * Sends SVSNICKs as appropriate and forces nickchanges too.
 */
int TreeSocket::DoCollision(User *u, time_t remotets, const char *remoteident, const char *remoteip, const char *remoteuid)
{
	/*
	 *  Under old protocol rules, we would have had to kill both clients.
	 *  Really, this sucks.
	 * These days, we have UID. And, so what we do is, force nick change client(s)
	 * involved according to timestamp rules.
	 *
	 * RULES: 	 
	 *  user@ip equal: 	 
	 *   Force nick change on OLDER timestamped client 	 
	 *  user@ip differ: 	 
	 *   Force nick change on NEWER timestamped client 	 
	 *  TS EQUAL: 	 
	 *   FNC both. 	 
	 * 	 
	 * This stops abusive use of collisions, simplifies problems with loops, and so on. 	 
	 *   -- w00t
	 */
	bool bChangeLocal = true;
	bool bChangeRemote = true;

	/* for brevity, don't use the User */
	time_t localts = u->age;
	const char *localident = u->ident;
	const char *localip = u->GetIPString();

	/* mmk. let's do this again. */
	if (remotets == localts)
	{
		/* equal. fuck them both! do nada, let the handler at the bottom figure this out. */
	}
	else
	{
		/* fuck. now it gets complex. */

		/* first, let's see if ident@host matches. */
		bool SamePerson = !strcmp(localident, remoteident)
				&& !strcmp(localip, remoteip);

		/*
		 * if ident@ip is equal, and theirs is newer, or
		 * ident@ip differ, and ours is newer
		 */
		if((SamePerson && remotets < localts) ||
		   (!SamePerson && remotets > localts))
		{
			/* remote needs to change */
			bChangeLocal = false;
		}
		else
		{
			/* ours needs to change */
			bChangeRemote = false;
		}
	}


	if (bChangeLocal)
	{
		u->ForceNickChange(u->uuid);

		if (!bChangeRemote)
			return 1;
	}
	if (bChangeRemote)
	{
		/*
		 * Cheat a little here. Instead of a dedicated command to change UID,
		 * use SVSNICK and accept their client with it's UID (as we know the SVSNICK will
		 * not fail under any circumstances -- UIDs are netwide exclusive).
		 *
		 * This means that each side of a collide will generate one extra NICK back to where
		 * they have just linked (and where it got the SVSNICK from), however, it will
		 * be dropped harmlessly as it will come in as :928AAAB NICK 928AAAB, and we already
		 * have 928AAAB's nick set to that.
		 *   -- w00t
		 */
		User *remote = this->Instance->FindUUID(remoteuid);

		if (remote)
		{
			/* buh.. nick change collide. force change their nick. */
			remote->ForceNickChange(remote->uuid);
		}
		else
		{
			/* user has not been introduced yet, just inform their server */
			this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" SVSNICK "+remoteuid+" " + remoteuid + " " + ConvToStr(remotets));
		}

		if (!bChangeLocal)
			return 2;
	}

	return 3;
}

bool TreeSocket::ParseUID(const std::string &source, std::deque<std::string> &params)
{
	/** Do we have enough parameters:
	 * UID uuid age nick host dhost ident +modestr ip.string :gecos
	 */
	if (params.size() != 10)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction ("+params[0]+" with only "+
				ConvToStr(params.size())+" of 10 parameters?)");
		return true;
	}

	time_t age = ConvToInt(params[1]);
	time_t signon = ConvToInt(params[8]);
	const char* tempnick = params[2].c_str();
	std::string empty;

	/* XXX probably validate UID length too -- w00t */
	cmd_validation valid[] = { {"Nickname", 2, NICKMAX}, {"Hostname", 3, 64}, {"Displayed hostname", 4, 64}, {"Ident", 5, IDENTMAX}, {"GECOS", 9, MAXGECOS}, {"", 0, 0} };

	TreeServer* remoteserver = Utils->FindServer(source);

	if (!remoteserver)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction (Unknown server "+source+")");
		return true;
	}

	/* Check parameters for validity before introducing the client, discovered by dmb */
	if (!age)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction (Invalid TS?)");
		return true;
	}

	for (size_t x = 0; valid[x].length; ++x)
	{
		if (params[valid[x].param].length() > valid[x].length)
		{
			this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction (" + valid[x].item + " > " + ConvToStr(valid[x].length) + ")");
			return true;
		}
	}


	/* check for collision */
	user_hash::iterator iter = this->Instance->clientlist->find(tempnick);

	if (iter != this->Instance->clientlist->end())
	{
		/*
		 * Nick collision.
		 */
		Instance->Log(DEBUG,"*** Collision on %s", tempnick);
		int collide = this->DoCollision(iter->second, age, params[5].c_str(), params[7].c_str(), params[0].c_str());

		if (collide == 2)
		{
			/* remote client changed, make sure we change their nick for the hash too */
			tempnick = params[0].c_str();
		}
	}

	/* IMPORTANT NOTE: For remote users, we pass the UUID in the constructor. This automatically
	 * sets it up in the UUID hash for us.
	 */
	User* _new = NULL;
	try
	{
		_new = new User(this->Instance, params[0]);
	}
	catch (...)
	{
		SendError("Protocol violation - Duplicate UUID '" + params[0] + "' on introduction of new user");
		return false;
	}
	(*(this->Instance->clientlist))[tempnick] = _new;
	_new->SetFd(FD_MAGIC_NUMBER);
	strlcpy(_new->nick, tempnick, NICKMAX - 1);
	strlcpy(_new->host, params[3].c_str(),64);
	strlcpy(_new->dhost, params[4].c_str(),64);
	_new->server = this->Instance->FindServerNamePtr(remoteserver->GetName().c_str());
	strlcpy(_new->ident, params[5].c_str(),IDENTMAX);
	strlcpy(_new->fullname, params[9].c_str(),MAXGECOS);
	_new->registered = REG_ALL;
	_new->signon = signon;
	_new->age = age;

	/* we need to remove the + from the modestring, so we can do our stuff */
	std::string::size_type pos_after_plus = params[6].find_first_not_of('+');
	if (pos_after_plus != std::string::npos)
	params[6] = params[6].substr(pos_after_plus);

	for (std::string::iterator v = params[6].begin(); v != params[6].end(); v++)
	{
		/* For each mode thats set, increase counter */
		ModeHandler* mh = Instance->Modes->FindMode(*v, MODETYPE_USER);

		if (mh)
		{
			mh->OnModeChange(_new, _new, NULL, empty, true);
			_new->SetMode(*v, true);
			mh->ChangeCount(1);
		}
	}

	/* now we've done with modes processing, put the + back for remote servers */
	params[6] = "+" + params[6];

#ifdef SUPPORT_IP6LINKS
	if (params[7].find_first_of(":") != std::string::npos)
		_new->SetSockAddr(AF_INET6, params[7].c_str(), 0);
	else
#endif
		_new->SetSockAddr(AF_INET, params[7].c_str(), 0);

	Instance->AddGlobalClone(_new);

	bool dosend = !(((this->Utils->quiet_bursts) && (this->bursting || Utils->FindRemoteBurstServer(remoteserver))) || (this->Instance->SilentULine(_new->server)));
	
	if (dosend)
		this->Instance->SNO->WriteToSnoMask('C',"Client connecting at %s: %s!%s@%s [%s] [%s]",_new->server,_new->nick,_new->ident,_new->host, _new->GetIPString(), _new->fullname);

	params[9] = ":" + params[9];
	Utils->DoOneToAllButSender(source, "UID", params, source);

	// Increment the Source Servers User Count..
	TreeServer* SourceServer = Utils->FindServer(source);
	if (SourceServer)
	{
		SourceServer->AddUserCount();
	}

	FOREACH_MOD_I(Instance,I_OnPostConnect,OnPostConnect(_new));

	return true;
}

/** Send one or more FJOINs for a channel of users.
 * If the length of a single line is more than 480-NICKMAX
 * in length, it is split over multiple lines.
 */
void TreeSocket::SendFJoins(TreeServer* Current, Channel* c)
{
	std::string buffer;
	char list[MAXBUF];
	std::string individual_halfops = std::string(":")+this->Instance->Config->GetSID()+" FMODE "+c->name+" "+ConvToStr(c->age);

	size_t dlen, curlen;
	dlen = curlen = snprintf(list,MAXBUF,":%s FJOIN %s %lu",this->Instance->Config->GetSID().c_str(),c->name,(unsigned long)c->age);
	int numusers = 0;
	char* ptr = list + dlen;

	CUList *ulist = c->GetUsers();
	std::string modes;
	std::string params;

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		// The first parameter gets a : before it
		size_t ptrlen = snprintf(ptr, MAXBUF, " %s%s,%s", !numusers ? ":" : "", c->GetAllPrefixChars(i->first), i->first->uuid);

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;

		if (curlen > (480-NICKMAX))
		{
			buffer.append(list).append("\r\n");
			dlen = curlen = snprintf(list,MAXBUF,":%s FJOIN %s %lu",this->Instance->Config->GetSID().c_str(),c->name,(unsigned long)c->age);
			ptr = list + dlen;
			ptrlen = 0;
			numusers = 0;
		}
	}

	if (numusers)
		buffer.append(list).append("\r\n");

	buffer.append(":").append(this->Instance->Config->GetSID()).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(c->ChanModes(true)).append("\r\n");

	int linesize = 1;
	for (BanList::iterator b = c->bans.begin(); b != c->bans.end(); b++)
	{
		int size = strlen(b->data) + 2;
		int currsize = linesize + size;
		if (currsize <= 350)
		{
			modes.append("b");
			params.append(" ").append(b->data);
			linesize += size; 
		}
		if ((params.length() >= MAXMODES) || (currsize > 350))
		{
			/* Wrap at MAXMODES */
			buffer.append(":").append(this->Instance->Config->GetSID()).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(modes).append(params).append("\r\n");
			modes.clear();
			params.clear();
			linesize = 1;
		}
	}

	/* Only send these if there are any */
	if (!modes.empty())
		buffer.append(":").append(this->Instance->Config->GetSID()).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(modes).append(params);

	this->WriteLine(buffer);
}

/** Send G, Q, Z and E lines */
void TreeSocket::SendXLines(TreeServer* Current)
{
	char data[MAXBUF];
	std::string buffer;
	std::string n = this->Instance->Config->GetSID();
	const char* sn = n.c_str();

	std::vector<std::string> types = Instance->XLines->GetAllTypes();

	for (std::vector<std::string>::iterator it = types.begin(); it != types.end(); ++it)
	{
		XLineLookup* lookup = Instance->XLines->GetAll(*it);

		if (lookup)
		{
			for (LookupIter i = lookup->begin(); i != lookup->end(); ++i)
			{
				snprintf(data,MAXBUF,":%s ADDLINE %s %s %s %lu %lu :%s\r\n",sn, it->c_str(), i->second->Displayable(),
						i->second->source,
						(unsigned long)i->second->set_time,
						(unsigned long)i->second->duration,
						i->second->reason);
				buffer.append(data);
			}
		}
	}

	if (!buffer.empty())
		this->WriteLine(buffer);
}

/** Send channel modes and topics */
void TreeSocket::SendChannelModes(TreeServer* Current)
{
	char data[MAXBUF];
	std::deque<std::string> list;
	std::string n = this->Instance->Config->GetSID();
	const char* sn = n.c_str();
	Instance->Log(DEBUG,"Sending channels and modes, %d to send", this->Instance->chanlist->size());
	for (chan_hash::iterator c = this->Instance->chanlist->begin(); c != this->Instance->chanlist->end(); c++)
	{
		SendFJoins(Current, c->second);
		if (*c->second->topic)
		{
			snprintf(data,MAXBUF,":%s FTOPIC %s %lu %s :%s",sn,c->second->name,(unsigned long)c->second->topicset,c->second->setby,c->second->topic);
			this->WriteLine(data);
		}
		FOREACH_MOD_I(this->Instance,I_OnSyncChannel,OnSyncChannel(c->second,(Module*)Utils->Creator,(void*)this));
		list.clear();
		c->second->GetExtList(list);
		for (unsigned int j = 0; j < list.size(); j++)
		{
			FOREACH_MOD_I(this->Instance,I_OnSyncChannelMetaData,OnSyncChannelMetaData(c->second,(Module*)Utils->Creator,(void*)this,list[j]));
		}
	}
}

/** send all users and their oper state/modes */
void TreeSocket::SendUsers(TreeServer* Current)
{
	char data[MAXBUF];
	std::deque<std::string> list;
	std::string dataline;
	for (user_hash::iterator u = this->Instance->clientlist->begin(); u != this->Instance->clientlist->end(); u++)
	{
		if (u->second->registered == REG_ALL)
		{
			TreeServer* theirserver = Utils->FindServer(u->second->server);
			if (theirserver)
			{
				snprintf(data,MAXBUF,":%s UID %s %lu %s %s %s %s +%s %s %lu :%s", theirserver->GetID().c_str(), u->second->uuid,
						(unsigned long)u->second->age, u->second->nick, u->second->host, u->second->dhost,
						u->second->ident, u->second->FormatModes(), u->second->GetIPString(),
						(unsigned long)u->second->signon, u->second->fullname);
				this->WriteLine(data);
				if (*u->second->oper)
				{
					snprintf(data,MAXBUF,":%s OPERTYPE %s", u->second->uuid, u->second->oper);
					this->WriteLine(data);
				}
				if (*u->second->awaymsg)
				{
					snprintf(data,MAXBUF,":%s AWAY :%s", u->second->uuid, u->second->awaymsg);
					this->WriteLine(data);
				}
			}

			FOREACH_MOD_I(this->Instance,I_OnSyncUser,OnSyncUser(u->second,(Module*)Utils->Creator,(void*)this));
			list.clear();
			u->second->GetExtList(list);
			for (unsigned int j = 0; j < list.size(); j++)
			{
				FOREACH_MOD_I(this->Instance,I_OnSyncUserMetaData,OnSyncUserMetaData(u->second,(Module*)Utils->Creator,(void*)this,list[j]));
			}
		}
	}
}

/** This function is called when we want to send a netburst to a local
 * server. There is a set order we must do this, because for example
 * users require their servers to exist, and channels require their
 * users to exist. You get the idea.
 */
void TreeSocket::DoBurst(TreeServer* s)
{
	std::string name = s->GetName();
	std::string burst = "BURST "+ConvToStr(Instance->Time(true));
	std::string endburst = "ENDBURST";
	this->Instance->SNO->WriteToSnoMask('l',"Bursting to \2%s\2 (Authentication: %s).", name.c_str(), this->GetTheirChallenge().empty() ? "plaintext password" : "SHA256-HMAC challenge-response");
	this->WriteLine(burst);
	/* send our version string */
	this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" VERSION :"+this->Instance->GetVersionString());
	/* Send server tree */
	this->SendServers(Utils->TreeRoot,s,1);
	/* Send users and their oper status */
	this->SendUsers(s);
	/* Send everything else (channel modes, xlines etc) */
	this->SendChannelModes(s);
	this->SendXLines(s);
	FOREACH_MOD_I(this->Instance,I_OnSyncOtherMetaData,OnSyncOtherMetaData((Module*)Utils->Creator,(void*)this));
	this->WriteLine(endburst);
	this->Instance->SNO->WriteToSnoMask('l',"Finished bursting to \2"+name+"\2.");
}

/** This function is called when we receive data from a remote
 * server. We buffer the data in a std::string (it doesnt stay
 * there for long), reading using BufferedSocket::Read() which can
 * read up to 16 kilobytes in one operation.
 *
 * IF THIS FUNCTION RETURNS FALSE, THE CORE CLOSES AND DELETES
 * THE SOCKET OBJECT FOR US.
 */
bool TreeSocket::OnDataReady()
{
	char* data = this->Read();
	/* Check that the data read is a valid pointer and it has some content */
	if (data && *data)
	{
		this->in_buffer.append(data);
		/* While there is at least one new line in the buffer,
		 * do something useful (we hope!) with it.
		 */
		while (in_buffer.find("\n") != std::string::npos)
		{
			std::string ret = in_buffer.substr(0,in_buffer.find("\n")-1);
			in_buffer = in_buffer.substr(in_buffer.find("\n")+1,in_buffer.length()-in_buffer.find("\n"));
			/* Use rfind here not find, as theres more
			 * chance of the \r being near the end of the
			 * string, not the start.
			 */
			if (ret.find("\r") != std::string::npos)
				ret = in_buffer.substr(0,in_buffer.find("\r")-1);
			/* Process this one, abort if it
			 * didnt return true.
			 */
			if (!this->ProcessLine(ret))
			{
				return false;
			}
		}
		return true;
	}
	/* EAGAIN returns an empty but non-NULL string, so this
	 * evaluates to TRUE for EAGAIN but to FALSE for EOF.
	 */
	return (data && !*data);
}
