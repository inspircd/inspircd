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
#include "socket.h"
#include "xline.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "resolvers.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

/* Create server sockets off a listener. */
void ServerSocketListener::OnAcceptReady(int newsock)
{
	bool found = false;
	int port;
	std::string incomingip;
	irc::sockets::satoap(&client, incomingip, port);

	found = (std::find(Utils->ValidIPs.begin(), Utils->ValidIPs.end(), incomingip) != Utils->ValidIPs.end());
	if (!found)
	{
		for (std::vector<std::string>::iterator i = Utils->ValidIPs.begin(); i != Utils->ValidIPs.end(); i++)
		{
			if (*i == "*" || irc::sockets::MatchCIDR(incomingip, *i))
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			ServerInstance->SNO->WriteToSnoMask('l', "Server connection from %s denied (no link blocks with that IP address)", incomingip.c_str());
			ServerInstance->SE->Close(newsock);
			return;
		}
	}

	/* we don't need to do anything with the pointer, creating it stores it in the necessary places */

	new TreeSocket(Utils, newsock, this, &client, &server);
}

/** Yay for fast searches!
 * This is hundreds of times faster than recursion
 * or even scanning a linked list, especially when
 * there are more than a few servers to deal with.
 * (read as: lots).
 */
TreeServer* SpanningTreeUtilities::FindServer(const std::string &ServerName)
{
	if (ServerInstance->IsSID(ServerName))
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

SpanningTreeUtilities::SpanningTreeUtilities(ModuleSpanningTree* C) : Creator(C)
{
	ServerInstance->Logs->Log("m_spanningtree",DEBUG,"***** Using SID for hash: %s *****", ServerInstance->Config->GetSID().c_str());

	this->TreeRoot = new TreeServer(this, ServerInstance->Config->ServerName, ServerInstance->Config->ServerDesc, ServerInstance->Config->GetSID());
	ServerUser = new FakeUser(TreeRoot->GetID());

	this->ReadConfiguration(true);
}

bool SpanningTreeUtilities::cull()
{
	for (unsigned int i = 0; i < Bindings.size(); i++)
	{
		Bindings[i]->cull();
	}

	while (TreeRoot->ChildCount())
	{
		TreeServer* child_server = TreeRoot->GetChild(0);
		if (child_server)
		{
			TreeSocket* sock = child_server->GetSocket();
			sock->Close();
			ServerInstance->GlobalCulls.AddItem(sock);
		}
	}

	ServerUser->uuid = TreeRoot->GetID();
	if (ServerUser->cull())
		delete ServerUser;
	return true;
}

SpanningTreeUtilities::~SpanningTreeUtilities()
{
	for (unsigned int i = 0; i < Bindings.size(); i++)
	{
		delete Bindings[i];
	}

	delete TreeRoot;
}

void SpanningTreeUtilities::AddThisServer(TreeServer* server, TreeServerList &list)
{
	if (list.find(server) == list.end())
		list[server] = server;
}

/* returns a list of DIRECT servernames for a specific channel */
void SpanningTreeUtilities::GetListOfServersForChannel(Channel* c, TreeServerList &list, char status, const CUList &exempt_list)
{
	const UserMembList *ulist = c->GetUsers();

	for (UserMembCIter i = ulist->begin(); i != ulist->end(); i++)
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

bool SpanningTreeUtilities::DoOneToAllButSenderRaw(const std::string &data, const std::string &omit, const std::string &prefix, const irc::string &command, parameterlist &params)
{
	TreeServer* omitroute = this->BestRouteTo(omit);
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

bool SpanningTreeUtilities::DoOneToAllButSender(const std::string &prefix, const std::string &command, parameterlist &params, std::string omit)
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

bool SpanningTreeUtilities::DoOneToMany(const std::string &prefix, const std::string &command, parameterlist &params)
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

bool SpanningTreeUtilities::DoOneToMany(const char* prefix, const char* command, parameterlist &params)
{
	std::string spfx = prefix;
	std::string scmd = command;
	return this->DoOneToMany(spfx, scmd, params);
}

bool SpanningTreeUtilities::DoOneToAllButSender(const char* prefix, const char* command, parameterlist &params, std::string omit)
{
	std::string spfx = prefix;
	std::string scmd = command;
	return this->DoOneToAllButSender(spfx, scmd, params, omit);
}

bool SpanningTreeUtilities::DoOneToOne(const std::string &prefix, const std::string &command, parameterlist &params, std::string target)
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
	for (std::vector<reference<Link> >::iterator i = LinkBlocks.begin(); i != LinkBlocks.end(); ++i)
	{
		Link* L = *i;
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
		start_type = DNS_QUERY_AAAA;
		if (strchr(L->IPAddr.c_str(),':'))
		{
			in6_addr n;
			if (inet_pton(AF_INET6, L->IPAddr.c_str(), &n) < 1)
				ipvalid = false;
		}
		else
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
				SecurityIPResolver* sr = new SecurityIPResolver(Creator, this, L->IPAddr, L, cached, start_type);
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
	ConfigReader* Conf = new ConfigReader;

	if (rebind)
	{
		for (unsigned int i = 0; i < Bindings.size(); i++)
		{
			delete Bindings[i];
		}
		Bindings.clear();

		for (int j = 0; j < Conf->Enumerate("bind"); j++)
		{
			std::string Type = Conf->ReadValue("bind","type",j);
			std::string IP = Conf->ReadValue("bind","address",j);
			std::string Port = Conf->ReadValue("bind","port",j);
			std::string ssl = Conf->ReadValue("bind","ssl",j);
			if (Type == "servers")
			{
				irc::portparser portrange(Port, false);
				int portno = -1;

				if (IP == "*")
					IP.clear();

				while ((portno = portrange.GetToken()))
				{
					ServerSocketListener *listener = new ServerSocketListener(this, portno, IP, ssl);
					if (listener->GetFd() == -1)
					{
						delete listener;
						continue;
					}

					Bindings.push_back(listener);
				}
			}
		}
	}
	FlatLinks = Conf->ReadFlag("security","flatlinks",0);
	HideULines = Conf->ReadFlag("security","hideulines",0);
	AnnounceTSChange = Conf->ReadFlag("options","announcets",0);
	AllowOptCommon = Conf->ReadFlag("options", "allowmismatch", 0);
	ChallengeResponse = !Conf->ReadFlag("security", "disablehmac", 0);
	quiet_bursts = Conf->ReadFlag("performance", "quietbursts", 0);
	PingWarnTime = Conf->ReadInteger("options", "pingwarning", 0, true);
	PingFreq = Conf->ReadInteger("options", "serverpingfreq", 0, true);

	if (PingFreq == 0)
		PingFreq = 60;

	if (PingWarnTime < 0 || PingWarnTime > PingFreq - 1)
		PingWarnTime = 0;

	AutoconnectBlocks.clear();
	LinkBlocks.clear();
	ValidIPs.clear();
	for (int j = 0; j < Conf->Enumerate("link"); ++j)
	{
		reference<Link> L = new Link;
		std::string Allow = Conf->ReadValue("link", "allowmask", j);
		L->Name = (Conf->ReadValue("link", "name", j)).c_str();
		L->AllowMask = Allow;
		L->IPAddr = Conf->ReadValue("link", "ipaddr", j);
		L->Port = Conf->ReadInteger("link", "port", j, true);
		L->SendPass = Conf->ReadValue("link", "sendpass", j);
		L->RecvPass = Conf->ReadValue("link", "recvpass", j);
		L->Fingerprint = Conf->ReadValue("link", "fingerprint", j);
		L->HiddenFromStats = Conf->ReadFlag("link", "statshidden", j);
		L->Timeout = Conf->ReadInteger("link", "timeout", j, true);
		L->Hook = Conf->ReadValue("link", "transport", j);
		L->Bind = Conf->ReadValue("link", "bind", j);
		L->Hidden = Conf->ReadFlag("link", "hidden", j);

		if (L->Name.find('.') == std::string::npos)
			throw CoreException("The link name '"+assign(L->Name)+"' is invalid and must contain at least one '.' character");

		if (L->Name.length() > 64)
			throw CoreException("The link name '"+assign(L->Name)+"' is longer than 64 characters!");

		if ((!L->IPAddr.empty()) && (!L->RecvPass.empty()) && (!L->SendPass.empty()) && (!L->Name.empty()) && (L->Port))
		{
			if (Allow.length())
				ValidIPs.push_back(Allow);

			ValidIPs.push_back(L->IPAddr);

			/* Needs resolving */
			bool ipvalid = true;
			QueryType start_type = DNS_QUERY_A;
			start_type = DNS_QUERY_AAAA;
			if (strchr(L->IPAddr.c_str(),':'))
			{
				in6_addr n;
				if (inet_pton(AF_INET6, L->IPAddr.c_str(), &n) < 1)
					ipvalid = false;
			}
			else
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
					SecurityIPResolver* sr = new SecurityIPResolver(Creator, this, L->IPAddr, L, cached, start_type);
					ServerInstance->AddResolver(sr, cached);
				}
				catch (...)
				{
				}
			}
		}
		else
		{
			if (L->IPAddr.empty())
			{
				L->IPAddr = "*";
				ValidIPs.push_back("*");
				ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Configuration warning: Link block " + assign(L->Name) + " has no IP defined! This will allow any IP to connect as this server, and MAY not be what you want.");
			}

			if (L->RecvPass.empty())
			{
				throw CoreException("Invalid configuration for server '"+assign(L->Name)+"', recvpass not defined!");
			}

			if (L->SendPass.empty())
			{
				throw CoreException("Invalid configuration for server '"+assign(L->Name)+"', sendpass not defined!");
			}

			if (L->Name.empty())
			{
				throw CoreException("Invalid configuration, link tag without a name! IP address: "+L->IPAddr);
			}

			if (!L->Port)
			{
				ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Configuration warning: Link block " + assign(L->Name) + " has no port defined, you will not be able to /connect it.");
			}
		}

		LinkBlocks.push_back(L);
	}

	for (int j = 0; j < Conf->Enumerate("autoconnect"); ++j)
	{
		reference<Autoconnect> A = new Autoconnect;
		A->Period = Conf->ReadInteger("autoconnect", "period", j, true);
		A->NextConnectTime = ServerInstance->Time() + A->Period;
		A->position = -1;
		std::string servers = Conf->ReadValue("autoconnect", "server", j);
		irc::spacesepstream ss(servers);
		std::string server;
		while (ss.GetToken(server))
		{
			A->servers.push_back(server);
		}

		if (A->Period <= 0)
		{
			throw CoreException("Invalid configuration for autoconnect, period not a positive integer!");
		}

		if (A->servers.empty())
		{
			throw CoreException("Invalid configuration for autoconnect, server cannot be empty!");
		}

		AutoconnectBlocks.push_back(A);
	}

	delete Conf;
}

Link* SpanningTreeUtilities::FindLink(const std::string& name)
{
	for (std::vector<reference<Link> >::iterator i = LinkBlocks.begin(); i != LinkBlocks.end(); ++i)
	{
		Link* x = *i;
		if (InspIRCd::Match(x->Name.c_str(), name.c_str()))
		{
			return x;
		}
	}
	return NULL;
}
