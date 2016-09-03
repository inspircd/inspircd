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

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"
#include "resolvers.h"
#include "commandbuilder.h"

SpanningTreeUtilities* Utils = NULL;

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
			new TreeSocket(newsock, from, client, server);
			return MOD_RES_ALLOW;
		}
	}
	ServerInstance->SNO->WriteToSnoMask('l', "Server connection from %s denied (no link blocks with that IP address)", incomingip.c_str());
	return MOD_RES_DENY;
}

TreeServer* SpanningTreeUtilities::FindServer(const std::string &ServerName)
{
	if (InspIRCd::IsSID(ServerName))
		return this->FindServerID(ServerName);

	server_hash::iterator iter = serverlist.find(ServerName);
	if (iter != serverlist.end())
	{
		return iter->second;
	}
	else
	{
		return NULL;
	}
}

/** Find the first server matching a given glob mask.
 * We iterate over the list and match each one until we get a hit.
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

TreeServer* SpanningTreeUtilities::FindRouteTarget(const std::string& target)
{
	TreeServer* const server = FindServer(target);
	if (server)
		return server;

	User* const user = ServerInstance->FindNick(target);
	if (user)
		return TreeServer::Get(user);

	return NULL;
}

SpanningTreeUtilities::SpanningTreeUtilities(ModuleSpanningTree* C)
	: Creator(C), TreeRoot(NULL)
	, PingFreq(60) // XXX: TreeServer constructor reads this and TreeRoot is created before the config is read, so init it to something (value doesn't matter) to avoid a valgrind warning in TimerManager on unload
{
	ServerInstance->Timers.AddTimer(&RefreshTimer);
}

CullResult SpanningTreeUtilities::cull()
{
	const TreeServer::ChildServers& children = TreeRoot->GetChildren();
	while (!children.empty())
	{
		TreeSocket* sock = children.front()->GetSocket();
		sock->Close();
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

// Returns a list of DIRECT servers for a specific channel
void SpanningTreeUtilities::GetListOfServersForChannel(Channel* c, TreeSocketSet& list, char status, const CUList& exempt_list)
{
	unsigned int minrank = 0;
	if (status)
	{
		PrefixMode* mh = ServerInstance->Modes->FindPrefix(status);
		if (mh)
			minrank = mh->GetPrefixRank();
	}

	const Channel::MemberMap& ulist = c->GetUsers();
	for (Channel::MemberMap::const_iterator i = ulist.begin(); i != ulist.end(); ++i)
	{
		if (IS_LOCAL(i->first))
			continue;

		if (minrank && i->second->getRank() < minrank)
			continue;

		if (exempt_list.find(i->first) == exempt_list.end())
		{
			TreeServer* best = TreeServer::Get(i->first);
			list.insert(best->GetSocket());
		}
	}
	return;
}

void SpanningTreeUtilities::DoOneToAllButSender(const CmdBuilder& params, TreeServer* omitroute)
{
	const std::string& FullLine = params.str();

	const TreeServer::ChildServers& children = TreeRoot->GetChildren();
	for (TreeServer::ChildServers::const_iterator i = children.begin(); i != children.end(); ++i)
	{
		TreeServer* Route = *i;
		// Send the line if the route isn't the path to the one to be omitted
		if (Route != omitroute)
		{
			Route->GetSocket()->WriteLine(FullLine);
		}
	}
}

void SpanningTreeUtilities::DoOneToOne(const CmdBuilder& params, Server* server)
{
	TreeServer* ts = static_cast<TreeServer*>(server);
	TreeSocket* sock = ts->GetSocket();
	if (sock)
		sock->WriteLine(params);
}

void SpanningTreeUtilities::RefreshIPCache()
{
	ValidIPs.clear();
	for (std::vector<reference<Link> >::iterator i = LinkBlocks.begin(); i != LinkBlocks.end(); ++i)
	{
		Link* L = *i;
		if (!L->Port)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Ignoring a link block without a port.");
			/* Invalid link block */
			continue;
		}

		ValidIPs.insert(ValidIPs.end(), L->AllowMasks.begin(), L->AllowMasks.end());

		irc::sockets::sockaddrs dummy;
		bool ipvalid = irc::sockets::aptosa(L->IPAddr, L->Port, dummy);
		if ((L->IPAddr == "*") || (ipvalid))
			ValidIPs.push_back(L->IPAddr);
		else if (this->Creator->DNS)
		{
			SecurityIPResolver* sr = new SecurityIPResolver(Creator, *this->Creator->DNS, L->IPAddr, L, DNS::QUERY_AAAA);
			try
			{
				this->Creator->DNS->Process(sr);
			}
			catch (DNS::Exception &)
			{
				delete sr;
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

		irc::spacesepstream sep = tag->getString("allowmask");
		for (std::string s; sep.GetToken(s);)
			L->AllowMasks.push_back(s);

		L->IPAddr = tag->getString("ipaddr");
		L->Port = tag->getInt("port");
		L->SendPass = tag->getString("sendpass", tag->getString("password"));
		L->RecvPass = tag->getString("recvpass", tag->getString("password"));
		L->Fingerprint = tag->getString("fingerprint");
		L->HiddenFromStats = tag->getBool("statshidden");
		L->Timeout = tag->getDuration("timeout", 30);
		L->Hook = tag->getString("ssl");
		L->Bind = tag->getString("bind");
		L->Hidden = tag->getBool("hidden");

		if (L->Name.empty())
			throw ModuleException("Invalid configuration, found a link tag without a name!" + (!L->IPAddr.empty() ? " IP address: "+L->IPAddr : ""));

		if (L->Name.find('.') == std::string::npos)
			throw ModuleException("The link name '"+L->Name+"' is invalid as it must contain at least one '.' character");

		if (L->Name.length() > ServerInstance->Config->Limits.MaxHost)
			throw ModuleException("The link name '"+L->Name+"' is invalid as it is longer than " + ConvToStr(ServerInstance->Config->Limits.MaxHost) + " characters");

		if (L->RecvPass.empty())
			throw ModuleException("Invalid configuration for server '"+L->Name+"', recvpass not defined");

		if (L->SendPass.empty())
			throw ModuleException("Invalid configuration for server '"+L->Name+"', sendpass not defined");

		if ((L->SendPass.find(' ') != std::string::npos) || (L->RecvPass.find(' ') != std::string::npos))
			throw ModuleException("Link block '" + L->Name + "' has a password set that contains a space character which is invalid");

		if ((L->SendPass[0] == ':') || (L->RecvPass[0] == ':'))
			throw ModuleException("Link block '" + L->Name + "' has a password set that begins with a colon (:) which is invalid");

		if (L->IPAddr.empty())
		{
			L->IPAddr = "*";
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Configuration warning: Link block '" + L->Name + "' has no IP defined! This will allow any IP to connect as this server, and MAY not be what you want.");
		}

		if (!L->Port)
			ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Configuration warning: Link block '" + L->Name + "' has no port defined, you will not be able to /connect it.");

		L->Fingerprint.erase(std::remove(L->Fingerprint.begin(), L->Fingerprint.end(), ':'), L->Fingerprint.end());
		LinkBlocks.push_back(L);
	}

	tags = ServerInstance->Config->ConfTags("autoconnect");
	for(ConfigIter i = tags.first; i != tags.second; ++i)
	{
		ConfigTag* tag = i->second;
		reference<Autoconnect> A = new Autoconnect(tag);
		A->Period = tag->getDuration("period", 60, 1);
		A->NextConnectTime = ServerInstance->Time() + A->Period;
		A->position = -1;
		irc::spacesepstream ss(tag->getString("server"));
		std::string server;
		while (ss.GetToken(server))
		{
			A->servers.push_back(server);
		}

		if (A->servers.empty())
		{
			throw ModuleException("Invalid configuration for autoconnect, server cannot be empty!");
		}

		AutoconnectBlocks.push_back(A);
	}

	for (server_hash::const_iterator i = serverlist.begin(); i != serverlist.end(); ++i)
		i->second->CheckULine();

	RefreshIPCache();
}

Link* SpanningTreeUtilities::FindLink(const std::string& name)
{
	for (std::vector<reference<Link> >::iterator i = LinkBlocks.begin(); i != LinkBlocks.end(); ++i)
	{
		Link* x = *i;
		if (InspIRCd::Match(x->Name, name, ascii_case_insensitive_map))
		{
			return x;
		}
	}
	return NULL;
}

void SpanningTreeUtilities::SendChannelMessage(const std::string& prefix, Channel* target, const std::string& text, char status, const CUList& exempt_list, const char* message_type, TreeSocket* omit)
{
	CmdBuilder msg(prefix, message_type);
	msg.push_raw(' ');
	if (status != 0)
		msg.push_raw(status);
	msg.push_raw(target->name).push_last(text);

	TreeSocketSet list;
	this->GetListOfServersForChannel(target, list, status, exempt_list);
	for (TreeSocketSet::iterator i = list.begin(); i != list.end(); ++i)
	{
		TreeSocket* Sock = *i;
		if (Sock != omit)
			Sock->WriteLine(msg);
	}
}
