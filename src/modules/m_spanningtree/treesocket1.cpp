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

#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "inspircd.h"
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

/** Because most of the I/O gubbins are encapsulated within
 * InspSocket, we just call the superclass constructor for
 * most of the action, and append a few of our own values
 * to it.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, Module* HookMod)
	: InspSocket(SI, host, port, listening, maxtime), Utils(Util), Hook(HookMod)
{
	myhost = host;
	this->LinkState = LISTENER;
	theirchallenge = ourchallenge = "";
	if (listening && Hook)
		InspSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
}

TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, std::string host, int port, bool listening, unsigned long maxtime, const std::string &ServerName, const std::string &bindto, Module* HookMod)
	: InspSocket(SI, host, port, listening, maxtime, bindto), Utils(Util), Hook(HookMod)
{
	myhost = ServerName;
	theirchallenge = ourchallenge = "";
	this->LinkState = CONNECTING;
	if (Hook)
		InspSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
}

/** When a listening socket gives us a new file descriptor,
 * we must associate it with a socket without creating a new
 * connection. This constructor is used for this purpose.
 */
TreeSocket::TreeSocket(SpanningTreeUtilities* Util, InspIRCd* SI, int newfd, char* ip, Module* HookMod)
	: InspSocket(SI, newfd, ip), Utils(Util), Hook(HookMod)
{
	this->LinkState = WAIT_AUTH_1;
	theirchallenge = ourchallenge = "";
	/* If we have a transport module hooked to the parent, hook the same module to this
	 * socket, and set a timer waiting for handshake before we send CAPAB etc.
	 */
	if (Hook)
	{
		InspSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
		Instance->Timers->AddTimer(new HandshakeTimer(Instance, this, &(Utils->LinkBlocks[0]), this->Utils));
	}
	else
	{
		/* Otherwise, theres no lower layer transport in plain TCP/IP,
		 * so just send the capabilities right now.
		 */
		this->SendCapabilities();
	}
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
		InspSocketUnhookRequest(this, (Module*)Utils->Creator, Hook).Send();
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
				this->Instance->SNO->WriteToSnoMask('l',"Connection to \2"+myhost+"\2["+(x->HiddenFromStats ? "<hidden>" : this->GetIP())+"] started.");
				if (Hook)
				{
					InspSocketHookRequest(this, (Module*)Utils->Creator, Hook).Send();
					this->Instance->SNO->WriteToSnoMask('l',"Connection to \2"+myhost+"\2["+(x->HiddenFromStats ? "<hidden>" : this->GetIP())+"] using transport \2"+x->Hook+"\2");
				}
				else
					this->SendCapabilities();
				/* found who we're supposed to be connecting to, send the neccessary gubbins. */
				/*if (Hook)*/
					Instance->Timers->AddTimer(new HandshakeTimer(Instance, this, &(*x), this->Utils));
				/*else
					this->WriteLine(std::string("SERVER ")+this->Instance->Config->ServerName+" "+x->SendPass+" 0 :"+this->Instance->Config->ServerDesc);*/
				return true;
			}
		}
	}
	/* There is a (remote) chance that between the /CONNECT and the connection
	 * being accepted, some muppet has removed the <link> block and rehashed.
	 * If that happens the connection hangs here until it's closed. Unlikely
	 * and rather harmless.
	 */
	this->Instance->SNO->WriteToSnoMask('l',"Connection to \2"+myhost+"\2 lost link tag(!)");
	return true;
}

void TreeSocket::OnError(InspSocketError e)
{
	/* We don't handle this method, because all our
	 * dirty work is done in OnClose() (see below)
	 * which is still called on error conditions too.
	 */
	if (e == I_ERR_CONNECT)
	{
		this->Instance->SNO->WriteToSnoMask('l',"Connection failed: Connection to \002"+myhost+"\002 refused");
		Link* MyLink = Utils->FindLink(myhost);
		if (MyLink)
			Utils->DoFailOver(MyLink);
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
			snprintf(command,1024,":%s SERVER %s * %d :%s",Current->GetName().c_str(),recursive_server->GetName().c_str(),hops,recursive_server->GetDesc().c_str());
			this->WriteLine(command);
			this->WriteLine(":"+recursive_server->GetName()+" VERSION :"+recursive_server->GetVersion());
			/* down to next level */
			this->SendServers(recursive_server, s, hops+1);
		}
	}
}

std::string TreeSocket::MyCapabilities()
{
	std::vector<std::string> modlist;
	std::string capabilities = "";
	for (int i = 0; i <= this->Instance->GetModuleCount(); i++)
	{
		if (this->Instance->modules[i]->GetVersion().Flags & VF_COMMON)
			modlist.push_back(this->Instance->Config->module_names[i]);
	}
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
	std::string out;
	for (unsigned int i = 0; i < length; i++)
		out += static_cast<char>((rand() % 26) + 65);
	return out;
}

void TreeSocket::SendCapabilities()
{
	irc::commasepstream modulelist(MyCapabilities());
	this->WriteLine("CAPAB START");

	/* Send module names, split at 509 length */
	std::string item = "*";
	std::string line = "CAPAB MODULES ";
	while ((item = modulelist.GetToken()) != "")
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
	this->SetOurChallenge(RandString(20));
	this->WriteLine("CAPAB CAPABILITIES :NICKMAX="+ConvToStr(NICKMAX)+" HALFOP="+ConvToStr(this->Instance->Config->AllowHalfop)+" CHANMAX="+ConvToStr(CHANMAX)+" MAXMODES="+ConvToStr(MAXMODES)+" IDENTMAX="+ConvToStr(IDENTMAX)+" MAXQUIT="+ConvToStr(MAXQUIT)+" MAXTOPIC="+ConvToStr(MAXTOPIC)+" MAXKICK="+ConvToStr(MAXKICK)+" MAXGECOS="+ConvToStr(MAXGECOS)+" MAXAWAY="+ConvToStr(MAXAWAY)+" IP6NATIVE="+ConvToStr(ip6)+" IP6SUPPORT="+ConvToStr(ip6support)+" PROTOCOL="+ConvToStr(ProtocolVersion)+" CHALLENGE="+this->GetOurChallenge());

	this->WriteLine("CAPAB END");
}

/* Check a comma seperated list for an item */
bool TreeSocket::HasItem(const std::string &list, const std::string &item)
{
	irc::commasepstream seplist(list);
	std::string item2 = "*";
	while ((item2 = seplist.GetToken()) != "")
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
	std::string item = "*";
	std::string result = "";
	while ((item = list_one.GetToken()) != "")
	{
		if (!HasItem(two, item))
		{
			result.append(" ");
			result.append(item);
		}
	}
	return result;
}

bool TreeSocket::Capab(const std::deque<std::string> &params)
{
	if (params.size() < 1)
	{
		this->WriteLine("ERROR :Invalid number of parameters for CAPAB - Mismatched version");
		return false;
	}
	if (params[0] == "START")
	{
		this->ModuleList = "";
		this->CapKeys.clear();
	}
	else if (params[0] == "END")
	{
		std::string reason = "";
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
		if (((this->CapKeys.find("IP6SUPPORT") == this->CapKeys.end()) && (ip6support)) || ((this->CapKeys.find("IP6SUPPORT") != this->CapKeys.end()) && (this->CapKeys.find("IP6SUPPORT")->second != ConvToStr(ip6support))))
			reason = "We don't both support linking to IPV6 servers";
		if (((this->CapKeys.find("IP6NATIVE") != this->CapKeys.end()) && (this->CapKeys.find("IP6NATIVE")->second == "1")) && (!ip6support))
			reason = "The remote server is IPV6 native, and we don't support linking to IPV6 servers";
		if (((this->CapKeys.find("NICKMAX") == this->CapKeys.end()) || ((this->CapKeys.find("NICKMAX") != this->CapKeys.end()) && (this->CapKeys.find("NICKMAX")->second != ConvToStr(NICKMAX)))))
			reason = "Maximum nickname lengths differ or remote nickname length not specified";
		if (((this->CapKeys.find("PROTOCOL") == this->CapKeys.end()) || ((this->CapKeys.find("PROTOCOL") != this->CapKeys.end()) && (this->CapKeys.find("PROTOCOL")->second != ConvToStr(ProtocolVersion)))))
		{
			if (this->CapKeys.find("PROTOCOL") != this->CapKeys.end())
			{
				reason = "Mismatched protocol versions "+this->CapKeys.find("PROTOCOL")->second+" and "+ConvToStr(ProtocolVersion);
			}
			else
			{
				reason = "Protocol version not specified";
			}
		}
		if (((this->CapKeys.find("HALFOP") == this->CapKeys.end()) && (Instance->Config->AllowHalfop)) || ((this->CapKeys.find("HALFOP") != this->CapKeys.end()) && (this->CapKeys.find("HALFOP")->second != ConvToStr(Instance->Config->AllowHalfop))))
			reason = "We don't both have halfop support enabled/disabled identically";
		if (((this->CapKeys.find("IDENTMAX") == this->CapKeys.end()) || ((this->CapKeys.find("IDENTMAX") != this->CapKeys.end()) && (this->CapKeys.find("IDENTMAX")->second != ConvToStr(IDENTMAX)))))
			reason = "Maximum ident lengths differ or remote ident length not specified";
		if (((this->CapKeys.find("CHANMAX") == this->CapKeys.end()) || ((this->CapKeys.find("CHANMAX") != this->CapKeys.end()) && (this->CapKeys.find("CHANMAX")->second != ConvToStr(CHANMAX)))))
			reason = "Maximum channel lengths differ or remote channel length not specified";
		if (((this->CapKeys.find("MAXMODES") == this->CapKeys.end()) || ((this->CapKeys.find("MAXMODES") != this->CapKeys.end()) && (this->CapKeys.find("MAXMODES")->second != ConvToStr(MAXMODES)))))
			reason = "Maximum modes per line differ or remote modes per line not specified";
		if (((this->CapKeys.find("MAXQUIT") == this->CapKeys.end()) || ((this->CapKeys.find("MAXQUIT") != this->CapKeys.end()) && (this->CapKeys.find("MAXQUIT")->second != ConvToStr(MAXQUIT)))))
			reason = "Maximum quit lengths differ or remote quit length not specified";
		if (((this->CapKeys.find("MAXTOPIC") == this->CapKeys.end()) || ((this->CapKeys.find("MAXTOPIC") != this->CapKeys.end()) && (this->CapKeys.find("MAXTOPIC")->second != ConvToStr(MAXTOPIC)))))
			reason = "Maximum topic lengths differ or remote topic length not specified";
		if (((this->CapKeys.find("MAXKICK") == this->CapKeys.end()) || ((this->CapKeys.find("MAXKICK") != this->CapKeys.end()) && (this->CapKeys.find("MAXKICK")->second != ConvToStr(MAXKICK)))))
			reason = "Maximum kick lengths differ or remote kick length not specified";
		if (((this->CapKeys.find("MAXGECOS") == this->CapKeys.end()) || ((this->CapKeys.find("MAXGECOS") != this->CapKeys.end()) && (this->CapKeys.find("MAXGECOS")->second != ConvToStr(MAXGECOS)))))
			reason = "Maximum GECOS (fullname) lengths differ or remote GECOS length not specified";
		if (((this->CapKeys.find("MAXAWAY") == this->CapKeys.end()) || ((this->CapKeys.find("MAXAWAY") != this->CapKeys.end()) && (this->CapKeys.find("MAXAWAY")->second != ConvToStr(MAXAWAY)))))
			reason = "Maximum awaymessage lengths differ or remote awaymessage length not specified";

		/* Challenge response, store their challenge for our password */
		std::map<std::string,std::string>::iterator n = this->CapKeys.find("CHALLENGE");
		if (n != this->CapKeys.end())
		{
			/* Challenge-response is on now */
			this->SetTheirChallenge(n->second);
		}

		if (reason.length())
		{
			this->WriteLine("ERROR :CAPAB negotiation failed: "+reason);
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
			this->Instance->WriteOpers("Server \002"+Current->GetName()+"\002 split: "+reason);
		}
		else
		{
			this->Instance->WriteOpers("Server \002"+Current->GetName()+"\002 split from server \002"+Current->GetParent()->GetName()+"\002 with reason: "+reason);
		}
		num_lost_servers = 0;
		num_lost_users = 0;
		std::string from = Current->GetParent()->GetName()+" "+Current->GetName();
		SquitServer(from, Current);
		Current->Tidy();
		Current->GetParent()->DelChild(Current);
		DELETE(Current);
		this->Instance->WriteOpers("Netsplit complete, lost \002%d\002 users on \002%d\002 servers.", num_lost_users, num_lost_servers);
	}
	else
	{
		Instance->Log(DEFAULT,"Squit from unknown server");
	}
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
	userrec* who = this->Instance->FindNick(source);
	if (who)
	{
		/* FMODE from a user, set sourceserv to the users server name */
		sourceserv = who->server;
	}
	else
	{
		/* FMODE from a server, create a fake user to receive mode feedback */
		who = new userrec(this->Instance);
		who->SetFd(FD_MAGIC_NUMBER);
		smode = true;	   /* Setting this flag tells us we should free the userrec later */
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
	/* Extract the TS value of the object, either userrec or chanrec */
	userrec* dst = this->Instance->FindNick(params[0]);
	chanrec* chan = NULL;
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

	/* TS is equal or less: Merge the mode changes into ours and pass on.
	 */
	if (TS <= ourTS)
	{
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

	if (smode)
		DELETE(who);

	return true;
}

/** FTOPIC command */
bool TreeSocket::ForceTopic(const std::string &source, std::deque<std::string> &params)
{
	if (params.size() != 4)
		return true;
	time_t ts = atoi(params[1].c_str());
	std::string nsource = source;
	chanrec* c = this->Instance->FindChan(params[0]);
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
				userrec* user = this->Instance->FindNick(source);
				if (!user)
				{
					c->WriteChannelWithServ(source.c_str(), "TOPIC %s :%s", c->name, c->topic);
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
	 *
	 * If the timestamps are NOT equal, the losing side removes all privilage
	 * modes from all of its users that currently exist in the channel, before
	 * introducing new users into the channel which are listed in the FJOIN
	 * command's parameters. This means, all modes +ohv, and privilages added
	 * by modules, such as +qa. The losing side then LOWERS its timestamp value
	 * of the channel to match that of the winning side, and the modes of the
	 * users of the winning side are merged in with the losing side. The loser
	 * then sends out a set of FMODE commands which 'confirm' that it just
	 * removed all privilage modes from its existing users, which allows for
	 * services packages to still work correctly without needing to know the
	 * timestamping rules which InspIRCd follows. In TS6 servers this is always
	 * a problem, and services packages must contain code which explicitly
	 * behaves as TS6 does, removing ops from the losing side of a split where
	 * neccessary within its internal records, as this state information is
	 * not explicitly echoed out in that protocol.
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

	char first[MAXBUF];	     /* The first parameter of the mode command */
	char modestring[MAXBUF];	/* The mode sequence (2nd parameter) of the mode command */
	char* mode_users[127];	  /* The values used by the mode command */
	memset(&mode_users,0,sizeof(mode_users));       /* Initialize mode parameters */
	mode_users[0] = first;	  /* Set this up to be our on-stack value */
	mode_users[1] = modestring;     /* Same here as above */
	strcpy(modestring,"+");	 /* Initialize the mode sequence to just '+' */
	unsigned int modectr = 2;       /* Pointer to the third mode parameter (e.g. the one after the +-sequence) */

	userrec* who = NULL;		    /* User we are currently checking */
	std::string channel = params[0];	/* Channel name, as a string */
	time_t TS = atoi(params[1].c_str());    /* Timestamp given to us for remote side */
	std::string nicklist = params[2];
	bool created = false;

	/* Try and find the channel */
	chanrec* chan = this->Instance->FindChan(channel);

	/* Initialize channel name in the mode parameters */
	strlcpy(mode_users[0],channel.c_str(),MAXBUF);

	/* default TS is a high value, which if we dont have this
	 * channel will let the other side apply their modes.
	 */
	time_t ourTS = Instance->Time(true)+600;
	/* Does this channel exist? if it does, get its REAL timestamp */
	if (chan)
		ourTS = chan->age;
	else
		created = true; /* don't perform deops, and set TS to correct time after processing. */

	/* do this first, so our mode reversals are correctly received by other servers
	 * if there is a TS collision.
	 */
	params[2] = ":" + params[2];
	Utils->DoOneToAllButSender(source,"FJOIN",params,source);

	/* In 1.1, if they have the newer channel, we immediately clear
	 * all status modes from our users. We then accept their modes.
	 * If WE have the newer channel its the other side's job to do this.
	 * Note that this causes the losing server to send out confirming
	 * FMODE lines.
	 */
	if (ourTS > TS)
	{
		std::deque<std::string> param_list;
		/* Lower the TS here */
		if (Utils->AnnounceTSChange && chan)
			chan->WriteChannelWithServ(Instance->Config->ServerName,
			"NOTICE %s :TS for %s changed from %lu to %lu", chan->name, chan->name, ourTS, TS);
		ourTS = TS;
		/* Zap all the privilage modes on our side, if the channel exists here */
		if (!created)
		{
			param_list.push_back(channel);
			/* Do this first! */
			chan->age = TS;
			this->RemoveStatus(Instance->Config->ServerName, param_list);
		}
	}
	/* Put the final parameter of the FJOIN into a tokenstream ready to split it */
	irc::tokenstream users(nicklist);
	std::string item;

	/* Now, process every 'prefixes,nick' pair */
	while (users.GetToken(item))
	{
		/* Find next user */
		const char* usr = item.c_str();
		/* Safety check just to make sure someones not sent us an FJOIN full of spaces
		 * (is this even possible?) */
		if (usr && *usr)
		{
			const char* permissions = usr;
			int ntimes = 0;
			char* nm = new char[MAXBUF];
			char* tnm = nm;
			/* Iterate through all the prefix values, convert them from prefixes
			 * to mode letters, and append them to the mode sequence
			 */
			while ((*permissions) && (*permissions != ',') && (ntimes < MAXBUF))
			{
				ModeHandler* mh = Instance->Modes->FindPrefix(*permissions);
				if (mh)
				{
					/* This is a valid prefix */
					ntimes++;
					*tnm++ = mh->GetModeChar();
				}
				else
				{
					/* Not a valid prefix...
					 * danger bill bobbertson! (that's will robinsons older brother ;-) ...)
					 */
					this->Instance->WriteOpers("ERROR: We received a user with an unknown prefix '%c'. Closed connection to avoid a desync.",*permissions);
					this->WriteLine(std::string("ERROR :Invalid prefix '")+(*permissions)+"' in FJOIN");
					return false;
				}
				usr++;
				permissions++;
			}
			/* Null terminate modes */
			*tnm = 0;
			/* Advance past the comma, to the nick */
			usr++;
			/* Check the user actually exists */
			who = this->Instance->FindNick(usr);
			if (who)
			{
				/* Check that the user's 'direction' is correct
				 * based on the server sending the FJOIN. We must
				 * check each nickname in turn, because the origin of
				 * the FJOIN may be different to the origin of the nicks
				 * in the command itself.
				 */
				TreeServer* route_back_again = Utils->BestRouteTo(who->server);
				if ((!route_back_again) || (route_back_again->GetSocket() != this))
				{
					/* Oh dear oh dear. */
					delete[] nm;
					continue;
				}

				/* NOTE: Moved this below the fake direction check, so that modes
				 * arent put into the mode list for users that were collided, and
				 * may reconnect from the other side or our side before the split
				 * is completed!
				 */

				/* Did they get any modes? How many times? */
				strlcat(modestring, nm, MAXBUF);
				for (int k = 0; k < ntimes; k++)
					mode_users[modectr++] = strdup(usr);
				/* Free temporary buffer used for mode sequence */
				delete[] nm;

				/* Finally, we can actually place the user into the channel.
				 * We're sure its right. Final answer, phone a friend.
				 */
				if (created)
					chanrec::JoinUser(this->Instance, who, channel.c_str(), true, "", TS);
				else
					chanrec::JoinUser(this->Instance, who, channel.c_str(), true, "");
				/* Have we already queued up MAXMODES modes with parameters
				 * (+qaohv) ready to be sent to the server?
				 */
				if (modectr >= (MAXMODES-1))
				{
					/* Only actually give the users any status if we lost
					 * the FJOIN or drew (equal timestamps).
					 * It isn't actually possible for ourTS to be > TS here,
					 * only possible to actually have ourTS == TS, or
					 * ourTS < TS, because if we lost, we already lowered
					 * our TS above before we entered this loop. We only
					 * check >= as a safety measure, in case someone stuffed
					 * up. If someone DID stuff up, it was most likely me.
					 * Note: I do not like baseball bats in the face...
					 */
					if (ourTS >= TS)
					{
						this->Instance->SendMode((const char**)mode_users,modectr,who);

						/* Something stuffed up, and for some reason, the timestamp is
						 * NOT lowered right now and should be. Lower it. Usually this
						 * code won't be executed, doubtless someone will remove it some
						 * day soon.
						 */
						if (ourTS > TS)
						{
							Instance->Log(DEFAULT,"Channel TS for %s changed from %lu to %lu",chan->name,ourTS,TS);
							chan->age = TS;
							ourTS = TS;
						}
					}

					/* Reset all this back to defaults, and
					 * free any ram we have left allocated.
					 */
					strcpy(mode_users[1],"+");
					for (unsigned int f = 2; f < modectr; f++)
						free(mode_users[f]);
					modectr = 2;
				}
			}
			else
			{
				/* Remember to free this */
				delete[] nm;
				/* If we got here, there's a nick in FJOIN which doesnt exist on this server.
				 * We don't try to process the nickname here (that WOULD cause a segfault because
				 * we'd be playing with null pointers) however, we DO pass the nickname on, just
				 * in case somehow we're desynched, so that other users which might be able to see
				 * the nickname get their fair chance to process it.
				 */
				Instance->Log(SPARSE,"Warning! Invalid user %s in FJOIN to channel %s IGNORED", usr, channel.c_str());
				continue;
			}
		}
	}

	/* there werent enough modes built up to flush it during FJOIN,
	 * or, there are a number left over. flush them out.
	 */
	if ((modectr > 2) && (who) && (chan))
	{
		if (ourTS >= TS)
		{
			/* Our channel is newer than theirs. Evil deeds must be afoot. */
			this->Instance->SendMode((const char**)mode_users,modectr,who);
			/* Yet again, we can't actually get a true value here, if everything else
			 * is working as it should.
			 */
			if (ourTS > TS)
			{
				Instance->Log(DEFAULT,"Channel TS for %s changed from %lu to %lu",chan->name,ourTS,TS);
				chan->age = TS;
				ourTS = TS;
			}
		}

		/* Free anything we have left to free */
		for (unsigned int f = 2; f < modectr; f++)
			free(mode_users[f]);
	}
	/* All done. That wasnt so bad was it, you can wipe
	 * the sweat from your forehead now. :-)
	 */
	return true;
}

/** NICK command */
bool TreeSocket::IntroduceClient(const std::string &source, std::deque<std::string> &params)
{
	/** Do we have enough parameters:
	 * NICK age nick host dhost ident +modes ip :gecos
	 */
	if (params.size() != 8)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" KILL "+params[1]+" :Invalid client introduction ("+params[1]+"?)");
		return true;
	}

	time_t age = atoi(params[0].c_str());
	const char* tempnick = params[1].c_str();

	/** Check parameters for validity before introducing the client, discovered by dmb.
	 * XXX: Can we make this neater?
	 */
	if (!age)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" KILL "+params[1]+" :Invalid client introduction (Invalid TS?)");
		return true;
	}
	else if (params[1].length() > NICKMAX)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" KILL "+params[1]+" :Invalid client introduction ("+params[1]+" > NICKMAX?)");
		return true;
	}
	else if (params[2].length() > 64)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" KILL "+params[1]+" :Invalid client introduction ("+params[2]+" > 64?)");
		return true;
	}
	else if (params[3].length() > 64)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" KILL "+params[1]+" :Invalid client introduction ("+params[3]+" > 64?)");
		return true;
	}
	else if (params[4].length() > IDENTMAX)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" KILL "+params[1]+" :Invalid client introduction ("+params[4]+" > IDENTMAX?)");
		return true;
	}
	else if (params[7].length() > MAXGECOS)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" KILL "+params[1]+" :Invalid client introduction ("+params[7]+" > MAXGECOS?)");
		return true;
	}

	/** Our client looks ok, lets introduce it now
	 */
	Instance->Log(DEBUG,"New remote client %s",tempnick);
	user_hash::iterator iter = this->Instance->clientlist->find(tempnick);

	if (iter != this->Instance->clientlist->end())
	{
		/* nick collision */
		this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" KILL "+tempnick+" :Nickname collision");
		userrec::QuitUser(this->Instance, iter->second, "Nickname collision");
		return true;
	}

	userrec* _new = new userrec(this->Instance);
	(*(this->Instance->clientlist))[tempnick] = _new;
	_new->SetFd(FD_MAGIC_NUMBER);
	strlcpy(_new->nick, tempnick,NICKMAX-1);
	strlcpy(_new->host, params[2].c_str(),63);
	strlcpy(_new->dhost, params[3].c_str(),63);
	_new->server = this->Instance->FindServerNamePtr(source.c_str());
	strlcpy(_new->ident, params[4].c_str(),IDENTMAX);
	strlcpy(_new->fullname, params[7].c_str(),MAXGECOS);
	_new->registered = REG_ALL;
	_new->signon = age;

	/* we need to remove the + from the modestring, so we can do our stuff */
	std::string::size_type pos_after_plus = params[5].find_first_not_of('+');
	if (pos_after_plus != std::string::npos)
	params[5] = params[5].substr(pos_after_plus);

	for (std::string::iterator v = params[5].begin(); v != params[5].end(); v++)
	{
		_new->modes[(*v)-65] = 1;
		/* For each mode thats set, increase counter */
		ModeHandler* mh = Instance->Modes->FindMode(*v, MODETYPE_USER);
		if (mh)
			mh->ChangeCount(1);
	}

	/* now we've done with modes processing, put the + back for remote servers */
	params[5] = "+" + params[5];

#ifdef SUPPORT_IP6LINKS
	if (params[6].find_first_of(":") != std::string::npos)
		_new->SetSockAddr(AF_INET6, params[6].c_str(), 0);
	else
#endif
		_new->SetSockAddr(AF_INET, params[6].c_str(), 0);

	Instance->AddGlobalClone(_new);

	if (!this->Instance->SilentULine(_new->server))
		this->Instance->SNO->WriteToSnoMask('C',"Client connecting at %s: %s!%s@%s [%s]",_new->server,_new->nick,_new->ident,_new->host, _new->GetIPString());

	params[7] = ":" + params[7];
	Utils->DoOneToAllButSender(source,"NICK", params, source);

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
void TreeSocket::SendFJoins(TreeServer* Current, chanrec* c)
{
	std::string buffer;
	char list[MAXBUF];
	std::string individual_halfops = std::string(":")+this->Instance->Config->ServerName+" FMODE "+c->name+" "+ConvToStr(c->age);

	size_t dlen, curlen;
	dlen = curlen = snprintf(list,MAXBUF,":%s FJOIN %s %lu",this->Instance->Config->ServerName,c->name,(unsigned long)c->age);
	int numusers = 0;
	char* ptr = list + dlen;

	CUList *ulist = c->GetUsers();
	std::string modes = "";
	std::string params = "";

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		// The first parameter gets a : before it
		size_t ptrlen = snprintf(ptr, MAXBUF, " %s%s,%s", !numusers ? ":" : "", c->GetAllPrefixChars(i->second), i->second->nick);

		curlen += ptrlen;
		ptr += ptrlen;

		numusers++;

		if (curlen > (480-NICKMAX))
		{
			buffer.append(list).append("\r\n");
			dlen = curlen = snprintf(list,MAXBUF,":%s FJOIN %s %lu",this->Instance->Config->ServerName,c->name,(unsigned long)c->age);
			ptr = list + dlen;
			ptrlen = 0;
			numusers = 0;
		}
	}

	if (numusers)
		buffer.append(list).append("\r\n");

	/* Sorry for the hax. Because newly created channels assume +nt,
	 * if this channel doesnt have +nt, explicitly send -n and -t for the missing modes.
	 */
	bool inverted = false;
	if (!c->IsModeSet('n'))
	{
		modes.append("-n");
		inverted = true;
	}
	if (!c->IsModeSet('t'))
	{
		modes.append("-t");
		inverted = true;
	}
	if (inverted)
	{
		modes.append("+");
	}

	buffer.append(":").append(this->Instance->Config->ServerName).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(c->ChanModes(true)).append("\r\n");

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
			buffer.append(":").append(this->Instance->Config->ServerName).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(modes).append(params).append("\r\n");
			modes = "";
			params = "";
			linesize = 1;
		}
	}

	/* Only send these if there are any */
	if (!modes.empty())
		buffer.append(":").append(this->Instance->Config->ServerName).append(" FMODE ").append(c->name).append(" ").append(ConvToStr(c->age)).append(" +").append(modes).append(params);

	this->WriteLine(buffer);
}

/** Send G, Q, Z and E lines */
void TreeSocket::SendXLines(TreeServer* Current)
{
	char data[MAXBUF];
	std::string buffer;
	std::string n = this->Instance->Config->ServerName;
	const char* sn = n.c_str();
	/* Yes, these arent too nice looking, but they get the job done */
	for (std::vector<ZLine*>::iterator i = Instance->XLines->zlines.begin(); i != Instance->XLines->zlines.end(); i++)
	{
		snprintf(data,MAXBUF,":%s ADDLINE Z %s %s %lu %lu :%s\r\n",sn,(*i)->ipaddr,(*i)->source,(unsigned long)(*i)->set_time,(unsigned long)(*i)->duration,(*i)->reason);
		buffer.append(data);
	}
	for (std::vector<QLine*>::iterator i = Instance->XLines->qlines.begin(); i != Instance->XLines->qlines.end(); i++)
	{
		snprintf(data,MAXBUF,":%s ADDLINE Q %s %s %lu %lu :%s\r\n",sn,(*i)->nick,(*i)->source,(unsigned long)(*i)->set_time,(unsigned long)(*i)->duration,(*i)->reason);
		buffer.append(data);
	}
	for (std::vector<GLine*>::iterator i = Instance->XLines->glines.begin(); i != Instance->XLines->glines.end(); i++)
	{
		snprintf(data,MAXBUF,":%s ADDLINE G %s@%s %s %lu %lu :%s\r\n",sn,(*i)->identmask,(*i)->hostmask,(*i)->source,(unsigned long)(*i)->set_time,(unsigned long)(*i)->duration,(*i)->reason);
		buffer.append(data);
	}
	for (std::vector<ELine*>::iterator i = Instance->XLines->elines.begin(); i != Instance->XLines->elines.end(); i++)
	{
		snprintf(data,MAXBUF,":%s ADDLINE E %s@%s %s %lu %lu :%s\r\n",sn,(*i)->identmask,(*i)->hostmask,(*i)->source,(unsigned long)(*i)->set_time,(unsigned long)(*i)->duration,(*i)->reason);
		buffer.append(data);
	}
	for (std::vector<ZLine*>::iterator i = Instance->XLines->pzlines.begin(); i != Instance->XLines->pzlines.end(); i++)
	{
		snprintf(data,MAXBUF,":%s ADDLINE Z %s %s %lu %lu :%s\r\n",sn,(*i)->ipaddr,(*i)->source,(unsigned long)(*i)->set_time,(unsigned long)(*i)->duration,(*i)->reason);
		buffer.append(data);
	}
	for (std::vector<QLine*>::iterator i = Instance->XLines->pqlines.begin(); i != Instance->XLines->pqlines.end(); i++)
	{
		snprintf(data,MAXBUF,":%s ADDLINE Q %s %s %lu %lu :%s\r\n",sn,(*i)->nick,(*i)->source,(unsigned long)(*i)->set_time,(unsigned long)(*i)->duration,(*i)->reason);
		buffer.append(data);
	}
	for (std::vector<GLine*>::iterator i = Instance->XLines->pglines.begin(); i != Instance->XLines->pglines.end(); i++)
	{
		snprintf(data,MAXBUF,":%s ADDLINE G %s@%s %s %lu %lu :%s\r\n",sn,(*i)->identmask,(*i)->hostmask,(*i)->source,(unsigned long)(*i)->set_time,(unsigned long)(*i)->duration,(*i)->reason);
		buffer.append(data);
	}
	for (std::vector<ELine*>::iterator i = Instance->XLines->pelines.begin(); i != Instance->XLines->pelines.end(); i++)
	{
		snprintf(data,MAXBUF,":%s ADDLINE E %s@%s %s %lu %lu :%s\r\n",sn,(*i)->identmask,(*i)->hostmask,(*i)->source,(unsigned long)(*i)->set_time,(unsigned long)(*i)->duration,(*i)->reason);
		buffer.append(data);
	}

	if (!buffer.empty())
		this->WriteLine(buffer);
}

/** Send channel modes and topics */
void TreeSocket::SendChannelModes(TreeServer* Current)
{
	char data[MAXBUF];
	std::deque<std::string> list;
	std::string n = this->Instance->Config->ServerName;
	const char* sn = n.c_str();
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
			snprintf(data,MAXBUF,":%s NICK %lu %s %s %s %s +%s %s :%s",u->second->server,(unsigned long)u->second->age,u->second->nick,u->second->host,u->second->dhost,u->second->ident,u->second->FormatModes(),u->second->GetIPString(),u->second->fullname);
			this->WriteLine(data);
			if (*u->second->oper)
			{
				snprintf(data,MAXBUF,":%s OPERTYPE %s", u->second->nick, u->second->oper);
				this->WriteLine(data);
			}
			if (*u->second->awaymsg)
			{
				snprintf(data,MAXBUF,":%s AWAY :%s", u->second->nick, u->second->awaymsg);
				this->WriteLine(data);
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
	std::string burst = "BURST "+ConvToStr(Instance->Time(true));
	std::string endburst = "ENDBURST";
	// Because by the end of the netburst, it  could be gone!
	std::string name = s->GetName();
	this->Instance->SNO->WriteToSnoMask('l',"Bursting to \2"+name+"\2.");
	this->WriteLine(burst);
	/* send our version string */
	this->WriteLine(std::string(":")+this->Instance->Config->ServerName+" VERSION :"+this->Instance->GetVersionString());
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
 * there for long), reading using InspSocket::Read() which can
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

