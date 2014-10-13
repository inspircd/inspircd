/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "socket.h"
#include "xline.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "resolvers.h"

/* Create server sockets off a listener. */
ModResult ModuleSpanningTree::OnAcceptConnection(int newsock, ListenSocket* from, irc::sockets::sockaddrs* client, irc::sockets::sockaddrs* server)
{
	if (from->bind_tag->getString("type") != "servers")
		return MOD_RES_PASSTHRU;

	std::string incomingip = client->addr();

	for (std::vector<std::string>::iterator i = Utils->ValidIPs.begin(); i != Utils->ValidIPs.end(); i++)
	{
		if (*i == "*" || *i == incomingip || irc::sockets::cidr_mask(*i).match(*client))
		{
			/* we don't need to do anything with the pointer, creating it stores it in the necessary places */
			new TreeSocket(Utils, newsock, from, client, server);
			return MOD_RES_ALLOW;
		}
	}
	ServerInstance->SNO->WriteToSnoMask('l', "Server connection from %s denied (no link blocks with that IP address)", incomingip.c_str());
	return MOD_RES_DENY;
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

SpanningTreeUtilities::SpanningTreeUtilities(ModuleSpanningTree* C) : Creator(C)
{
	ServerInstance->Logs->Log("m_spanningtree",DEBUG,"***** Using SID for hash: %s *****", ServerInstance->Config->GetSID().c_str());

	this->TreeRoot = new TreeServer(this, ServerInstance->Config->ServerName, ServerInstance->Config->ServerDesc, ServerInstance->Config->GetSID());
	this->ReadConfiguration();
}

CullResult SpanningTreeUtilities::cull()
{
	while (TreeRoot->ChildCount())
	{
		TreeServer* child_server = TreeRoot->GetChild(0);
		if (child_server)
		{
			TreeSocket* sock = child_server->GetSocket();
			sock->Close();
		}
	}

	for(std::map<TreeSocket*, std::pair<std::string, int> >::iterator i = timeoutlist.begin(); i != timeoutlist.end(); ++i)
	{
		TreeSocket* s = i->first;
		s->Close();
	}
	TreeRoot->cull();

	return classbase::cull();
}

SpanningTreeUtilities::~SpanningTreeUtilities()
{
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
	unsigned int minrank = 0;
	if (status)
	{
		ModeHandler* mh = ServerInstance->Modes->FindPrefix(status);
		if (mh)
			minrank = mh->GetPrefixRank();
	}

	const UserMembList *ulist = c->GetUsers();

	for (UserMembCIter i = ulist->begin(); i != ulist->end(); i++)
	{
		if (IS_LOCAL(i->first))
			continue;

		if (minrank && i->second->getRank() < minrank)
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

bool SpanningTreeUtilities::DoOneToAllButSender(const std::string &prefix, const std::string &command, const parameterlist &params, const std::string& omit)
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

bool SpanningTreeUtilities::DoOneToMany(const std::string &prefix, const std::string &command, const parameterlist &params)
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

bool SpanningTreeUtilities::DoOneToOne(const std::string &prefix, const std::string &command, const parameterlist &params, const std::string& target)
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
		if (!L->Port)
		{
			ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"m_spanningtree: Ignoring a link block without a port.");
			/* Invalid link block */
			continue;
		}

		if (L->AllowMask.length())
			ValidIPs.push_back(L->AllowMask);

		irc::sockets::sockaddrs dummy;
		bool ipvalid = irc::sockets::aptosa(L->IPAddr, L->Port, dummy);
		if ((L->IPAddr == "*") || (ipvalid))
			ValidIPs.push_back(L->IPAddr);
		else
		{
			try
			{
				bool cached = false;
				SecurityIPResolver* sr = new SecurityIPResolver(Creator, this, L->IPAddr, L, cached, DNS_QUERY_AAAA);
				ServerInstance->AddResolver(sr, cached);
			}
			catch (...)
			{
			}
		}
	}
}

void SpanningTreeUtilities::ReadConfiguration()
{
	ConfigTag* security = ServerInstance->Config->ConfValue("security");
	ConfigTag* options = ServerInstance->Config->ConfValue("options");
	FlatLinks = security->getBool("flatlinks");
	HideULines = security->getBool("hideulines");
	AnnounceTSChange = options->getBool("announcets");
	AllowOptCommon = options->getBool("allowmismatch");
	ChallengeResponse = !security->getBool("disablehmac");
	quiet_bursts = ServerInstance->Config->ConfValue("performance")->getBool("quietbursts");
	PingWarnTime = options->getInt("pingwarning");
	PingFreq = options->getInt("serverpingfreq");

	if (PingFreq == 0)
		PingFreq = 60;

	if (PingWarnTime < 0 || PingWarnTime > PingFreq - 1)
		PingWarnTime = 0;

	AutoconnectBlocks.clear();
	LinkBlocks.clear();
	ConfigTagList tags = ServerInstance->Config->ConfTags("link");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		reference<Link> L = new Link(tag);
		std::string linkname = tag->getString("name");
		L->Name = linkname.c_str();
		L->AllowMask = tag->getString("allowmask");
		L->IPAddr = tag->getString("ipaddr");
		L->Port = tag->getInt("port");
		L->SendPass = tag->getString("sendpass", tag->getString("password"));
		L->RecvPass = tag->getString("recvpass", tag->getString("password"));
		L->Fingerprint = tag->getString("fingerprint");
		L->HiddenFromStats = tag->getBool("statshidden");
		L->Timeout = tag->getInt("timeout", 30);
		L->Hook = tag->getString("ssl");
		L->Bind = tag->getString("bind");
		L->Hidden = tag->getBool("hidden");

		if (L->Name.empty())
			throw ModuleException("Invalid configuration, found a link tag without a name!" + (!L->IPAddr.empty() ? " IP address: "+L->IPAddr : ""));

		if (L->Name.find('.') == std::string::npos)
			throw ModuleException("The link name '"+assign(L->Name)+"' is invalid as it must contain at least one '.' character");

		if (L->Name.length() > 64)
			throw ModuleException("The link name '"+assign(L->Name)+"' is invalid as it is longer than 64 characters");

		if (L->RecvPass.empty())
			throw ModuleException("Invalid configuration for server '"+assign(L->Name)+"', recvpass not defined");

		if (L->SendPass.empty())
			throw ModuleException("Invalid configuration for server '"+assign(L->Name)+"', sendpass not defined");

		if ((L->SendPass.find(' ') != std::string::npos) || (L->RecvPass.find(' ') != std::string::npos))
			throw ModuleException("Link block '" + assign(L->Name) + "' has a password set that contains a space character which is invalid");

		if ((L->SendPass[0] == ':') || (L->RecvPass[0] == ':'))
			throw ModuleException("Link block '" + assign(L->Name) + "' has a password set that begins with a colon (:) which is invalid");

		if (L->IPAddr.empty())
		{
			L->IPAddr = "*";
			ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Configuration warning: Link block '" + assign(L->Name) + "' has no IP defined! This will allow any IP to connect as this server, and MAY not be what you want.");
		}

		if (!L->Port)
			ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Configuration warning: Link block '" + assign(L->Name) + "' has no port defined, you will not be able to /connect it.");

		L->Fingerprint.erase(std::remove(L->Fingerprint.begin(), L->Fingerprint.end(), ':'), L->Fingerprint.end());
		LinkBlocks.push_back(L);
	}

	tags = ServerInstance->Config->ConfTags("autoconnect");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		reference<Autoconnect> A = new Autoconnect(tag);
		A->Period = tag->getInt("period");
		A->NextConnectTime = ServerInstance->Time() + A->Period;
		A->position = -1;
		irc::spacesepstream ss(tag->getString("server"));
		std::string server;
		while (ss.GetToken(server))
		{
			A->servers.push_back(server);
		}

		if (A->Period <= 0)
		{
			throw ModuleException("Invalid configuration for autoconnect, period not a positive integer!");
		}

		if (A->servers.empty())
		{
			throw ModuleException("Invalid configuration for autoconnect, server cannot be empty!");
		}

		AutoconnectBlocks.push_back(A);
	}

	RefreshIPCache();
}

Link* SpanningTreeUtilities::FindLink(const std::string& name)
{
	for (std::vector<reference<Link> >::iterator i = LinkBlocks.begin(); i != LinkBlocks.end(); ++i)
	{
		Link* x = *i;
		if (InspIRCd::Match(x->Name.c_str(), name.c_str(), rfc_case_insensitive_map))
		{
			return x;
		}
	}
	return NULL;
}

void SpanningTreeUtilities::Rehash()
{
	server_hash temp;
	for (server_hash::const_iterator i = serverlist.begin(); i != serverlist.end(); ++i)
		temp.insert(std::make_pair(i->first, i->second));
	serverlist.swap(temp);
	temp.clear();

	for (server_hash::const_iterator i = sidlist.begin(); i != sidlist.end(); ++i)
		temp.insert(std::make_pair(i->first, i->second));
	sidlist.swap(temp);
}
