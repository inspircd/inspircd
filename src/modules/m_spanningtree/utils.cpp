/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

/* Create server sockets off a listener. */
void ServerSocketListener::OnAcceptReady(const std::string &ipconnectedto, int newsock, const std::string &incomingip)
{
	bool found = false;
	char *ip = (char *)incomingip.c_str(); // XXX ugly cast

	found = (std::find(Utils->ValidIPs.begin(), Utils->ValidIPs.end(), ip) != Utils->ValidIPs.end());
	if (!found)
	{
		for (std::vector<std::string>::iterator i = Utils->ValidIPs.begin(); i != Utils->ValidIPs.end(); i++)
		{
			if (*i == "*" || irc::sockets::MatchCIDR(ip, *i))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			this->ServerInstance->SNO->WriteToSnoMask('l', "Server connection from %s denied (no link blocks with that IP address)", ip);
			ServerInstance->SE->Close(newsock);
			return;
		}
	}

	if (this->GetIOHook())
	{
		this->GetIOHook()->OnRawSocketAccept(newsock, incomingip.c_str(), this->bind_port);
	}

	/* we don't need a pointer to this, creating it stores it in the necessary places */
	new TreeSocket(this->Utils, this->ServerInstance, newsock, ip, this->GetIOHook());
	return;
}

/** Yay for fast searches!
 * This is hundreds of times faster than recursion
 * or even scanning a linked list, especially when
 * there are more than a few servers to deal with.
 * (read as: lots).
 */
TreeServer* SpanningTreeUtilities::FindServer(const std::string &ServerName)
{
	if (this->ServerInstance->IsSID(ServerName))
		return this->FindServerID(ServerName);

	server_hash::iterator iter = serverlist.find(ServerName.c_str());
	if (iter != serverlist.end())
	{
		return iter->second;
	}
	else
	{
		return NULL;
	}
}

/** Returns the locally connected server we must route a
 * message through to reach server 'ServerName'. This
 * only applies to one-to-one and not one-to-many routing.
 * See the comments for the constructor of TreeServer
 * for more details.
 */
TreeServer* SpanningTreeUtilities::BestRouteTo(const std::string &ServerName)
{
	if (ServerName.c_str() == TreeRoot->GetName() || ServerName == ServerInstance->Config->GetSID())
		return NULL;
	TreeServer* Found = FindServer(ServerName);
	if (Found)
	{
		return Found->GetRoute();
	}
	else
	{
		// Cheat a bit. This allows for (better) working versions of routing commands with nick based prefixes, without hassle
		User *u = ServerInstance->FindNick(ServerName);
		if (u)
		{
			Found = FindServer(u->server);
			if (Found)
				return Found->GetRoute();
		}

		return NULL;
	}
}

/** Find the first server matching a given glob mask.
 * Theres no find-using-glob method of hash_map [awwww :-(]
 * so instead, we iterate over the list using an iterator
 * and match each one until we get a hit. Yes its slow,
 * deal with it.
 */
TreeServer* SpanningTreeUtilities::FindServerMask(const std::string &ServerName)
{
	for (server_hash::iterator i = serverlist.begin(); i != serverlist.end(); i++)
	{
		if (InspIRCd::Match(i->first,ServerName))
			return i->second;
	}
	return NULL;
}

TreeServer* SpanningTreeUtilities::FindServerID(const std::string &id)
{
	server_hash::iterator iter = sidlist.find(id);
	if (iter != sidlist.end())
		return iter->second;
	else
		return NULL;
}

/* A convenient wrapper that returns true if a server exists */
bool SpanningTreeUtilities::IsServer(const std::string &ServerName)
{
	return (FindServer(ServerName) != NULL);
}

SpanningTreeUtilities::SpanningTreeUtilities(InspIRCd* Instance, ModuleSpanningTree* C) : ServerInstance(Instance), Creator(C)
{
	Bindings.clear();

	ServerInstance->Logs->Log("m_spanningtree",DEBUG,"***** Using SID for hash: %s *****", ServerInstance->Config->GetSID().c_str());

	this->TreeRoot = new TreeServer(this, ServerInstance, ServerInstance->Config->ServerName, ServerInstance->Config->ServerDesc, ServerInstance->Config->GetSID());
	this->ServerUser = new FakeUser(ServerInstance, TreeRoot->GetID());

	this->ReadConfiguration(true);
}

SpanningTreeUtilities::~SpanningTreeUtilities()
{
	for (std::vector<ServerSocketListener*>::iterator i = Bindings.begin(); i != Bindings.end(); ++i)
	{
		delete *i;
	}

	while (TreeRoot->ChildCount())
	{
		TreeServer* child_server = TreeRoot->GetChild(0);
		if (child_server)
		{
			TreeSocket* sock = child_server->GetSocket();
			ServerInstance->SE->DelFd(sock);
			sock->Close();
		}
	}
	
	// This avoids a collisions on reload
	ServerUser->uuid = TreeRoot->GetID();
	ServerInstance->Users->clientlist->erase(ServerUser->nick);
	ServerInstance->Users->uuidlist->erase(ServerUser->uuid);
	delete TreeRoot;
	delete ServerUser;
	ServerInstance->BufferedSocketCull();
}

void SpanningTreeUtilities::AddThisServer(TreeServer* server, TreeServerList &list)
{
	if (list.find(server) == list.end())
		list[server] = server;
}

/* returns a list of DIRECT servernames for a specific channel */
void SpanningTreeUtilities::GetListOfServersForChannel(Channel* c, TreeServerList &list, char status, const CUList &exempt_list)
{
	CUList *ulist = c->GetUsers();

	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if (IS_LOCAL(i->first))
			continue;

		if (status && !strchr(c->GetAllPrefixChars(i->first), status))
			continue;

		if (exempt_list.find(i->first) == exempt_list.end())
		{
			TreeServer* best = this->BestRouteTo(i->first->server);
			if (best)
				AddThisServer(best,list);
		}
	}
	return;
}

bool SpanningTreeUtilities::DoOneToAllButSenderRaw(const std::string &data, const std::string &omit, const std::string &prefix, const irc::string &command, std::deque<std::string> &params)
{
	char pfx = 0;
	TreeServer* omitroute = this->BestRouteTo(omit);
	if ((command == "NOTICE") || (command == "PRIVMSG"))
	{
		if (params.size() >= 2)
		{
			/* Prefixes */
			if (ServerInstance->Modes->FindPrefix(params[0][0]))
			{
				pfx = params[0][0];
				params[0] = params[0].substr(1, params[0].length()-1);
			}
			if ((*(params[0].c_str()) != '#') && (*(params[0].c_str()) != '$'))
			{
				// special routing for private messages/notices
				User* d = ServerInstance->FindNick(params[0]);
				if (d)
				{
					std::deque<std::string> par;
					par.push_back(params[0]);
					par.push_back(":"+params[1]);
					this->DoOneToOne(prefix,command.c_str(),par,d->server);
					return true;
				}
			}
			else if (*(params[0].c_str()) == '$')
			{
				std::deque<std::string> par;
				par.push_back(params[0]);
				par.push_back(":"+params[1]);
				this->DoOneToAllButSender(prefix,command.c_str(),par,omitroute->GetName());
				return true;
			}
			else
			{
				Channel* c = ServerInstance->FindChan(params[0]);
				User* u = ServerInstance->FindNick(prefix);
				if (c)
				{
					CUList elist;
					TreeServerList list;
					FOREACH_MOD(I_OnBuildExemptList, OnBuildExemptList((command == "PRIVMSG" ? MSG_PRIVMSG : MSG_NOTICE), c, u, pfx, elist, params[1]));
					GetListOfServersForChannel(c,list,pfx,elist);

					for (TreeServerList::iterator i = list.begin(); i != list.end(); i++)
					{
						TreeSocket* Sock = i->second->GetSocket();
						if ((Sock) && (i->second->GetName() != omit) && (omitroute != i->second))
						{
							Sock->WriteLine(data);
						}
					}
					return true;
				}
			}
		}
	}
	unsigned int items =this->TreeRoot->ChildCount();
	for (unsigned int x = 0; x < items; x++)
	{
		TreeServer* Route = this->TreeRoot->GetChild(x);
		if ((Route) && (Route->GetSocket()) && (Route->GetName() != omit) && (omitroute != Route))
		{
			TreeSocket* Sock = Route->GetSocket();
			if (Sock)
				Sock->WriteLine(data);
		}
	}
	return true;
}

bool SpanningTreeUtilities::DoOneToAllButSender(const std::string &prefix, const std::string &command, std::deque<std::string> &params, std::string omit)
{
	TreeServer* omitroute = this->BestRouteTo(omit);
	std::string FullLine = ":" + prefix + " " + command;
	for (std::deque<std::string>::const_iterator i = params.begin(); i != params.end(); ++i)
	{
		FullLine = FullLine + " " + *i;
	}
	unsigned int items = this->TreeRoot->ChildCount();
	for (unsigned int x = 0; x < items; x++)
	{
		TreeServer* Route = this->TreeRoot->GetChild(x);
		// Send the line IF:
		// The route has a socket (its a direct connection)
		// The route isnt the one to be omitted
		// The route isnt the path to the one to be omitted
		if ((Route) && (Route->GetSocket()) && (Route->GetName() != omit) && (omitroute != Route))
		{
			TreeSocket* Sock = Route->GetSocket();
			if (Sock)
				Sock->WriteLine(FullLine);
		}
	}
	return true;
}

bool SpanningTreeUtilities::DoOneToMany(const std::string &prefix, const std::string &command, std::deque<std::string> &params)
{
	std::string FullLine = ":" + prefix + " " + command;
	for (std::deque<std::string>::const_iterator i = params.begin(); i != params.end(); ++i)
	{
		FullLine = FullLine + " " + *i;
	}
	unsigned int items = this->TreeRoot->ChildCount();
	for (unsigned int x = 0; x < items; x++)
	{
		TreeServer* Route = this->TreeRoot->GetChild(x);
		if (Route && Route->GetSocket())
		{
			TreeSocket* Sock = Route->GetSocket();
			if (Sock)
				Sock->WriteLine(FullLine);
		}
	}
	return true;
}

bool SpanningTreeUtilities::DoOneToMany(const char* prefix, const char* command, std::deque<std::string> &params)
{
	std::string spfx = prefix;
	std::string scmd = command;
	return this->DoOneToMany(spfx, scmd, params);
}

bool SpanningTreeUtilities::DoOneToAllButSender(const char* prefix, const char* command, std::deque<std::string> &params, std::string omit)
{
	std::string spfx = prefix;
	std::string scmd = command;
	return this->DoOneToAllButSender(spfx, scmd, params, omit);
}

bool SpanningTreeUtilities::DoOneToOne(const std::string &prefix, const std::string &command, std::deque<std::string> &params, std::string target)
{
	TreeServer* Route = this->BestRouteTo(target);
	if (Route)
	{
		std::string FullLine = ":" + prefix + " " + command;
		for (std::deque<std::string>::const_iterator i = params.begin(); i != params.end(); ++i)
		{
			FullLine = FullLine + " " + *i;
		}
		if (Route && Route->GetSocket())
		{
			TreeSocket* Sock = Route->GetSocket();
			if (Sock)
				Sock->WriteLine(FullLine);
		}
		return true;
	}
	else
	{
		return false;
	}
}

void SpanningTreeUtilities::RefreshIPCache()
{
	ValidIPs.clear();
	for (std::vector<Link>::iterator L = LinkBlocks.begin(); L != LinkBlocks.end(); L++)
	{
		if (L->IPAddr.empty() || L->RecvPass.empty() || L->SendPass.empty() || L->Name.empty() || !L->Port)
		{
			if (L->Name.empty())
			{
				ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"m_spanningtree: Ignoring a malformed link block (all link blocks require a name!)");
			}
			else
			{
				ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"m_spanningtree: Ignoring a link block missing recvpass, sendpass, port or ipaddr.");
			}

			/* Invalid link block */
			continue;
		}

		ValidIPs.push_back(L->IPAddr);

		if (L->AllowMask.length())
			ValidIPs.push_back(L->AllowMask);

		/* Needs resolving */
		bool ipvalid = true;
		QueryType start_type = DNS_QUERY_A;
#ifdef IPV6
		start_type = DNS_QUERY_AAAA;
		if (strchr(L->IPAddr.c_str(),':'))
		{
			in6_addr n;
			if (inet_pton(AF_INET6, L->IPAddr.c_str(), &n) < 1)
				ipvalid = false;
		}
		else
#endif
		{
			in_addr n;
			if (inet_aton(L->IPAddr.c_str(),&n) < 1)
				ipvalid = false;
		}

		if (!ipvalid)
		{
			try
			{
				bool cached;
				SecurityIPResolver* sr = new SecurityIPResolver((Module*)this->Creator, this, ServerInstance, L->IPAddr, *L, cached, start_type);
				ServerInstance->AddResolver(sr, cached);
			}
			catch (...)
			{
			}
		}
	}
}

void SpanningTreeUtilities::ReadConfiguration(bool rebind)
{
	ConfigReader* Conf = new ConfigReader(ServerInstance);

	/* We don't need to worry about these being *unloaded* on the fly, only loaded,
	 * because we 'use' the interface locking the module in memory.
	 */
	hooks.clear();
	hooknames.clear();
	modulelist* ml = ServerInstance->Modules->FindInterface("BufferedSocketHook");

	/* Did we find any modules? */
	if (ml)
	{
		/* Yes, enumerate them all to find out the hook name */
		for (modulelist::iterator m = ml->begin(); m != ml->end(); m++)
		{
			/* Make a request to it for its name, its implementing
			 * BufferedSocketHook so we know its safe to do this
			 */
			std::string name = BufferedSocketNameRequest((Module*)Creator, *m).Send();
			/* Build a map of them */
			hooks[name.c_str()] = *m;
			hooknames.push_back(name);
		}
	}

	if (rebind)
	{
		for (std::vector<ServerSocketListener*>::iterator i = Bindings.begin(); i != Bindings.end(); ++i)
		{
			delete *i;
		}
		ServerInstance->BufferedSocketCull();
		Bindings.clear();

		for (int j = 0; j < Conf->Enumerate("bind"); j++)
		{
			std::string Type = Conf->ReadValue("bind","type",j);
			std::string IP = Conf->ReadValue("bind","address",j);
			std::string Port = Conf->ReadValue("bind","port",j);
			std::string transport = Conf->ReadValue("bind","transport",j);
			if (Type == "servers")
			{
				irc::portparser portrange(Port, false);
				int portno = -1;

				if (IP == "*")
					IP.clear();

				while ((portno = portrange.GetToken()))
				{
					if ((!transport.empty()) && (hooks.find(transport.c_str()) ==  hooks.end()))
					{
						throw CoreException("Can't find transport type '"+transport+"' for port "+IP+":"+Port+" - maybe you forgot to load it BEFORE m_spanningtree in your config file?");
						break;
					}

					ServerSocketListener *listener = new ServerSocketListener(ServerInstance, this, portno, (char *)IP.c_str());
					if (listener->GetFd() == -1)
					{
						delete listener;
						continue;
					}

					if (!transport.empty())
						listener->AddIOHook(hooks[transport.c_str()]);

					Bindings.push_back(listener);
				}
			}
		}
	}
	FlatLinks = Conf->ReadFlag("security","flatlinks",0);
	HideULines = Conf->ReadFlag("security","hideulines",0);
	AnnounceTSChange = Conf->ReadFlag("options","announcets",0);
	ChallengeResponse = !Conf->ReadFlag("security", "disablehmac", 0);
	quiet_bursts = Conf->ReadFlag("performance", "quietbursts", 0);
	PingWarnTime = Conf->ReadInteger("options", "pingwarning", 0, true);
	PingFreq = Conf->ReadInteger("options", "serverpingfreq", 0, true);

	if (PingFreq == 0)
		PingFreq = 60;

	if (PingWarnTime < 0 || PingWarnTime > PingFreq - 1)
		PingWarnTime = 0;

	LinkBlocks.clear();
	ValidIPs.clear();
	for (int j = 0; j < Conf->Enumerate("link"); j++)
	{
		Link L;
		std::string Allow = Conf->ReadValue("link", "allowmask", j);
		L.Name = (Conf->ReadValue("link", "name", j)).c_str();
		L.AllowMask = Allow;
		L.IPAddr = Conf->ReadValue("link", "ipaddr", j);
		L.FailOver = Conf->ReadValue("link", "failover", j).c_str();
		L.Port = Conf->ReadInteger("link", "port", j, true);
		L.SendPass = Conf->ReadValue("link", "sendpass", j);
		L.RecvPass = Conf->ReadValue("link", "recvpass", j);
		L.Fingerprint = Conf->ReadValue("link", "fingerprint", j);
		L.AutoConnect = Conf->ReadInteger("link", "autoconnect", j, true);
		L.HiddenFromStats = Conf->ReadFlag("link", "statshidden", j);
		L.Timeout = Conf->ReadInteger("link", "timeout", j, true);
		L.Hook = Conf->ReadValue("link", "transport", j);
		L.Bind = Conf->ReadValue("link", "bind", j);
		L.Hidden = Conf->ReadFlag("link", "hidden", j);

		if ((!L.Hook.empty()) && (hooks.find(L.Hook.c_str()) ==  hooks.end()))
		{
			throw CoreException("Can't find transport type '"+L.Hook+"' for link '"+assign(L.Name)+"' - maybe you forgot to load it BEFORE m_spanningtree in your config file? Skipping <link> tag completely.");
			continue;

		}

		// Fix: Only trip autoconnects if this wouldn't delay autoconnect..
		if (L.NextConnectTime > ((time_t)(ServerInstance->Time() + L.AutoConnect)))
			L.NextConnectTime = ServerInstance->Time() + L.AutoConnect;

		if (L.Name.find('.') == std::string::npos)
			throw CoreException("The link name '"+assign(L.Name)+"' is invalid and must contain at least one '.' character");

		if (L.Name.length() > 64)
			throw CoreException("The link name '"+assign(L.Name)+"' is longer than 64 characters!");

		if ((!L.IPAddr.empty()) && (!L.RecvPass.empty()) && (!L.SendPass.empty()) && (!L.Name.empty()) && (L.Port))
		{
			if (Allow.length())
				ValidIPs.push_back(Allow);

			ValidIPs.push_back(L.IPAddr);

			/* Needs resolving */
			bool ipvalid = true;
			QueryType start_type = DNS_QUERY_A;
#ifdef IPV6
			start_type = DNS_QUERY_AAAA;
			if (strchr(L.IPAddr.c_str(),':'))
			{
				in6_addr n;
				if (inet_pton(AF_INET6, L.IPAddr.c_str(), &n) < 1)
					ipvalid = false;
			}
			else
			{
				in_addr n;
				if (inet_aton(L.IPAddr.c_str(),&n) < 1)
					ipvalid = false;
			}
#else
			in_addr n;
			if (inet_aton(L.IPAddr.c_str(),&n) < 1)
				ipvalid = false;
#endif

			if (!ipvalid)
			{
				try
				{
					bool cached;
					SecurityIPResolver* sr = new SecurityIPResolver((Module*)this->Creator, this, ServerInstance, L.IPAddr, L, cached, start_type);
					ServerInstance->AddResolver(sr, cached);
				}
				catch (...)
				{
				}
			}
		}
		else
		{
			if (L.IPAddr.empty())
			{
				L.IPAddr = "*";
				ValidIPs.push_back("*");
				ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Configuration warning: Link block " + assign(L.Name) + " has no IP defined! This will allow any IP to connect as this server, and MAY not be what you want.");
			}

			if (L.RecvPass.empty())
			{
				throw CoreException("Invalid configuration for server '"+assign(L.Name)+"', recvpass not defined!");
			}

			if (L.SendPass.empty())
			{
				throw CoreException("Invalid configuration for server '"+assign(L.Name)+"', sendpass not defined!");
			}

			if (L.Name.empty())
			{
				throw CoreException("Invalid configuration, link tag without a name! IP address: "+L.IPAddr);
			}

			if (!L.Port)
			{
				ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Configuration warning: Link block " + assign(L.Name) + " has no port defined, you will not be able to /connect it.");
			}
		}

		LinkBlocks.push_back(L);
	}
	delete Conf;
}

void SpanningTreeUtilities::DoFailOver(Link* x)
{
	if (x->FailOver.length())
	{
		if (x->FailOver == x->Name)
		{
			this->ServerInstance->SNO->WriteToSnoMask('l', "FAILOVER: Some muppet configured the failover for server \002%s\002 to point at itself. Not following it!", x->Name.c_str());
			return;
		}
		Link* TryThisOne = this->FindLink(x->FailOver.c_str());
		if (TryThisOne)
		{
			TreeServer* CheckDupe = this->FindServer(x->FailOver.c_str());
			if (CheckDupe)
			{
				ServerInstance->Logs->Log("m_spanningtree",DEBUG,"Skipping existing failover: %s", x->FailOver.c_str());
			}
			else
			{
				this->ServerInstance->SNO->WriteToSnoMask('l', "FAILOVER: Trying failover link for \002%s\002: \002%s\002...", x->Name.c_str(), TryThisOne->Name.c_str());
				Creator->ConnectServer(TryThisOne);
			}
		}
		else
		{
			this->ServerInstance->SNO->WriteToSnoMask('l', "FAILOVER: Invalid failover server specified for server \002%s\002, will not follow!", x->Name.c_str());
		}
	}
}

Link* SpanningTreeUtilities::FindLink(const std::string& name)
{
	for (std::vector<Link>::iterator x = LinkBlocks.begin(); x != LinkBlocks.end(); x++)
	{
		if (InspIRCd::Match(x->Name.c_str(), name.c_str()))
		{
			return &(*x);
		}
	}
	return NULL;
}

void SpanningTreeUtilities::SendChannelMessage(const std::string& prefix, Channel* target, const std::string &text, char status, const CUList& exempt_list, const std::string& message_type)
{
	std::string raw = ":" + prefix + " " + message_type + " ";
	if (status)
		raw.append(1, status);
	raw += target->name + " :" + text;

	TreeServerList list;
	this->GetListOfServersForChannel(target, list, status, exempt_list);
	for (TreeServerList::iterator i = list.begin(); i != list.end(); ++i)
	{
		TreeSocket* Sock = i->second->GetSocket();
		if (Sock)
			Sock->WriteLine(raw);
	}
}
