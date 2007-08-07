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
#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
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

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

/** Yay for fast searches!
 * This is hundreds of times faster than recursion
 * or even scanning a linked list, especially when
 * there are more than a few servers to deal with.
 * (read as: lots).
 */
TreeServer* SpanningTreeUtilities::FindServer(const std::string &ServerName)
{
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

TreeServer* SpanningTreeUtilities::FindRemoteBurstServer(TreeServer* Server)
{
	server_hash::iterator iter = RemoteServersBursting.find(Server->GetName().c_str());
	if (iter != RemoteServersBursting.end())
		return iter->second;
	else
		return NULL;
}

TreeSocket* SpanningTreeUtilities::FindBurstingServer(const std::string &ServerName)
{
	std::map<irc::string,TreeSocket*>::iterator iter;
	iter = burstingserverlist.find(ServerName.c_str());
	if (iter != burstingserverlist.end())
	{
		return iter->second;
	}
	else
	{
		return NULL;
	}
}

void SpanningTreeUtilities::SetRemoteBursting(TreeServer* Server, bool bursting)
{
	server_hash::iterator iter = RemoteServersBursting.find(Server->GetName().c_str());
	if (bursting)
	{
		if (iter == RemoteServersBursting.end())
			RemoteServersBursting.insert(make_pair(Server->GetName(), Server));
		else return;
	}
	else
	{
		if (iter != RemoteServersBursting.end())
			RemoteServersBursting.erase(iter);
		else return;
	}
	ServerInstance->Log(DEBUG,"Server %s is %sbursting nicknames", Server->GetName().c_str(), bursting ? "" : "no longer ");
}

void SpanningTreeUtilities::AddBurstingServer(const std::string &ServerName, TreeSocket* s)
{
	std::map<irc::string,TreeSocket*>::iterator iter = burstingserverlist.find(ServerName.c_str());
	if (iter == burstingserverlist.end())
		burstingserverlist[ServerName.c_str()] = s;
}

void SpanningTreeUtilities::DelBurstingServer(TreeSocket* s)
{
	 for (std::map<irc::string,TreeSocket*>::iterator iter = burstingserverlist.begin(); iter != burstingserverlist.end(); iter++)
	 {
		 if (iter->second == s)
		 {
			 burstingserverlist.erase(iter);
			 return;
		 }
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
	if (ServerName.c_str() == TreeRoot->GetName())
		return NULL;
	TreeServer* Found = FindServer(ServerName);
	if (Found)
	{
		return Found->GetRoute();
	}
	else
	{
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
		if (match(i->first.c_str(),ServerName.c_str()))
			return i->second;
	}
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

	lines_to_apply = 0;

	this->TreeRoot = new TreeServer(this, ServerInstance, ServerInstance->Config->ServerName, ServerInstance->Config->ServerDesc);

	modulelist* ml = ServerInstance->FindInterface("InspSocketHook");

	/* Did we find any modules? */
	if (ml)
	{
		/* Yes, enumerate them all to find out the hook name */
		for (modulelist::iterator m = ml->begin(); m != ml->end(); m++)
		{
			/* Make a request to it for its name, its implementing
			 * InspSocketHook so we know its safe to do this
			 */
			std::string name = InspSocketNameRequest((Module*)Creator, *m).Send();
			/* Build a map of them */
			hooks[name.c_str()] = *m;
			hooknames.push_back(name);
		}
	}

	this->ReadConfiguration(true);
}

SpanningTreeUtilities::~SpanningTreeUtilities()
{
	for (unsigned int i = 0; i < Bindings.size(); i++)
	{
		ServerInstance->SE->DelFd(Bindings[i]);
		Bindings[i]->Close();
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
	delete TreeRoot;
	ServerInstance->InspSocketCull();
}

void SpanningTreeUtilities::AddThisServer(TreeServer* server, TreeServerList &list)
{
	if (list.find(server) == list.end())
		list[server] = server;
}

/* returns a list of DIRECT servernames for a specific channel */
void SpanningTreeUtilities::GetListOfServersForChannel(chanrec* c, TreeServerList &list, char status, const CUList &exempt_list)
{
	CUList *ulist;
	switch (status)
	{
		case '@':
			ulist = c->GetOppedUsers();
		break;
		case '%':
			ulist = c->GetHalfoppedUsers();
		break;
		case '+':
			ulist = c->GetVoicedUsers();
		break;
		default:
			ulist = c->GetUsers();
		break;
	}
	for (CUList::iterator i = ulist->begin(); i != ulist->end(); i++)
	{
		if ((i->first->GetFd() < 0) && (exempt_list.find(i->first) == exempt_list.end()))
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
			if ((*(params[0].c_str()) == '@') || (*(params[0].c_str()) == '%') || (*(params[0].c_str()) == '+'))
			{
				pfx = params[0][0];
				params[0] = params[0].substr(1, params[0].length()-1);
			}
			if ((*(params[0].c_str()) != '#') && (*(params[0].c_str()) != '$'))
			{
				// special routing for private messages/notices
				userrec* d = ServerInstance->FindNick(params[0]);
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
				chanrec* c = ServerInstance->FindChan(params[0]);
				userrec* u = ServerInstance->FindNick(prefix);
				if (c && u)
				{
					CUList elist;
					TreeServerList list;
					FOREACH_MOD(I_OnBuildExemptList, OnBuildExemptList((command == "PRIVMSG" ? MSG_PRIVMSG : MSG_NOTICE), c, u, pfx, elist));
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
	unsigned int words = params.size();
	for (unsigned int x = 0; x < words; x++)
	{
		FullLine = FullLine + " " + params[x];
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
	unsigned int words = params.size();
	for (unsigned int x = 0; x < words; x++)
	{
		FullLine = FullLine + " " + params[x];
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
		unsigned int words = params.size();
		for (unsigned int x = 0; x < words; x++)
		{
			FullLine = FullLine + " " + params[x];
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
		if ((!L->IPAddr.empty()) && (!L->RecvPass.empty()) && (!L->SendPass.empty()) && (!L->Name.empty()) && (L->Port))
		{
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
}

void SpanningTreeUtilities::ReadConfiguration(bool rebind)
{
	ConfigReader* Conf = new ConfigReader(ServerInstance);
	if (rebind)
	{
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
				while ((portno = portrange.GetToken()))
				{
					if (IP == "*")
						IP.clear();

					if ((!transport.empty()) && (hooks.find(transport.c_str()) ==  hooks.end()))
					{
						ServerInstance->Log(DEFAULT,"m_spanningtree: WARNING: Can't find transport type '%s' for port %s:%s - maybe you forgot to load it BEFORE m_spanningtree in your config file? - Skipping this port binding", transport.c_str(), IP.c_str(), Port.c_str());
						break;
					}

					TreeSocket* listener = new TreeSocket(this, ServerInstance, IP.c_str(), portno, true, 10, transport.empty() ? NULL : hooks[transport.c_str()]);
					if (listener->GetState() == I_LISTENING)
					{
						ServerInstance->Log(DEFAULT,"m_spanningtree: Binding server port %s:%d successful!", IP.c_str(), portno);
						Bindings.push_back(listener);
					}
					else
					{
						ServerInstance->Log(DEFAULT,"m_spanningtree: Warning: Failed to bind server port: %s:%d: %s",IP.c_str(), portno, strerror(errno));
						listener->Close();
					}
				}
			}
		}
	}
	FlatLinks = Conf->ReadFlag("options","flatlinks",0);
	HideULines = Conf->ReadFlag("options","hideulines",0);
	AnnounceTSChange = Conf->ReadFlag("options","announcets",0);
	EnableTimeSync = Conf->ReadFlag("timesync","enable",0);
	MasterTime = Conf->ReadFlag("timesync", "master", 0);
	ChallengeResponse = !Conf->ReadFlag("options", "disablehmac", 0);
	quiet_bursts = Conf->ReadFlag("options", "quietbursts", 0);
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
		L.AutoConnect = Conf->ReadInteger("link", "autoconnect", j, true);
		L.HiddenFromStats = Conf->ReadFlag("link", "statshidden", j);
		L.Timeout = Conf->ReadInteger("link", "timeout", j, true);
		L.Hook = Conf->ReadValue("link", "transport", j);
		L.Bind = Conf->ReadValue("link", "bind", j);
		L.Hidden = Conf->ReadFlag("link", "hidden", j);

		if ((!L.Hook.empty()) && (hooks.find(L.Hook.c_str()) ==  hooks.end()))
		{
			ServerInstance->Log(DEFAULT,"m_spanningtree: WARNING: Can't find transport type '%s' for link '%s' - maybe you forgot to load it BEFORE m_spanningtree in your config file? Skipping <link> tag completely.",
			L.Hook.c_str(), L.Name.c_str());
			continue;

		}

		L.NextConnectTime = time(NULL) + L.AutoConnect;
		/* Bugfix by brain, do not allow people to enter bad configurations */
		if (L.Name != ServerInstance->Config->ServerName)
		{
			if ((!L.IPAddr.empty()) && (!L.RecvPass.empty()) && (!L.SendPass.empty()) && (!L.Name.empty()) && (L.Port))
			{
				ValidIPs.push_back(L.IPAddr);

				if (Allow.length())
					ValidIPs.push_back(Allow);

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

				LinkBlocks.push_back(L);
			}
			else
			{
				if (L.IPAddr.empty())
				{
					ServerInstance->Log(DEFAULT,"Invalid configuration for server '%s', IP address not defined!",L.Name.c_str());
				}
				else if (L.RecvPass.empty())
				{
					ServerInstance->Log(DEFAULT,"Invalid configuration for server '%s', recvpass not defined!",L.Name.c_str());
				}
				else if (L.SendPass.empty())
				{
					ServerInstance->Log(DEFAULT,"Invalid configuration for server '%s', sendpass not defined!",L.Name.c_str());
				}
				else if (L.Name.empty())
				{
					ServerInstance->Log(DEFAULT,"Invalid configuration, link tag without a name!");
				}
				else if (!L.Port)
				{
					ServerInstance->Log(DEFAULT,"Invalid configuration for server '%s', no port specified!",L.Name.c_str());
				}
			}
		}
		else
		{
			ServerInstance->Log(DEFAULT,"Invalid configuration for server '%s', link tag has the same server name as the local server!",L.Name.c_str());
		}
	}
	DELETE(Conf);
}

void SpanningTreeUtilities::DoFailOver(Link* x)
{
	if (x->FailOver.length())
	{
		if (x->FailOver == x->Name)
		{
			ServerInstance->SNO->WriteToSnoMask('l',"FAILOVER: Some muppet configured the failover for server \002%s\002 to point at itself. Not following it!", x->Name.c_str());
			return;
		}
		Link* TryThisOne = this->FindLink(x->FailOver.c_str());
		if (TryThisOne)
		{
			ServerInstance->SNO->WriteToSnoMask('l',"FAILOVER: Trying failover link for \002%s\002: \002%s\002...", x->Name.c_str(), TryThisOne->Name.c_str());
			Creator->ConnectServer(TryThisOne);
		}
		else
		{
			ServerInstance->SNO->WriteToSnoMask('l',"FAILOVER: Invalid failover server specified for server \002%s\002, will not follow!", x->Name.c_str());
		}
	}
}

Link* SpanningTreeUtilities::FindLink(const std::string& name)
{
	for (std::vector<Link>::iterator x = LinkBlocks.begin(); x < LinkBlocks.end(); x++)
	{
		if (ServerInstance->MatchText(x->Name.c_str(), name.c_str()))
		{
			return &(*x);
		}
	}
	return NULL;
}

