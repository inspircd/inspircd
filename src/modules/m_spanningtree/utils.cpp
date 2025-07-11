/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014, 2018-2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
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
#include "listmode.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"
#include "resolvers.h"
#include "commandbuilder.h"

SpanningTreeUtilities* Utils = nullptr;

ModResult ModuleSpanningTree::OnAcceptConnection(int newsock, ListenSocket* from, const irc::sockets::sockaddrs& client, const irc::sockets::sockaddrs& server)
{
	if (!insp::equalsci(from->bind_tag->getString("type"), "servers"))
		return MOD_RES_PASSTHRU;

	const std::string incomingip = client.addr();
	for (const auto& validip : Utils->ValidIPs)
	{
		if (validip == "*" || validip == incomingip || irc::sockets::cidr_mask(validip).match(client))
		{
			/* we don't need to do anything with the pointer, creating it stores it in the necessary places */
			new TreeSocket(newsock, from, client, server);
			return MOD_RES_ALLOW;
		}
	}
	ServerInstance->SNO.WriteToSnoMask('l', "Server connection from {} denied (no link blocks with that IP address)", incomingip);
	return MOD_RES_DENY;
}

TreeServer* SpanningTreeUtilities::FindServer(const std::string& ServerName)
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
		return nullptr;
	}
}

/** Find the first server matching a given glob mask.
 * We iterate over the list and match each one until we get a hit.
 */
TreeServer* SpanningTreeUtilities::FindServerMask(const std::string& ServerName)
{
	for (const auto& [name, server] : serverlist)
	{
		if (InspIRCd::Match(name, ServerName))
			return server;
	}
	return nullptr;
}

TreeServer* SpanningTreeUtilities::FindServerID(const std::string& id)
{
	server_hash::iterator iter = sidlist.find(id);
	if (iter != sidlist.end())
		return iter->second;
	else
		return nullptr;
}

TreeServer* SpanningTreeUtilities::FindRouteTarget(const std::string& target)
{
	TreeServer* const server = FindServer(target);
	if (server)
		return server;

	User* const user = ServerInstance->Users.Find(target);
	if (user)
		return TreeServer::Get(user);

	return nullptr;
}

SpanningTreeUtilities::SpanningTreeUtilities(ModuleSpanningTree* C)
	: Creator(C)
{
	ServerInstance->Timers.AddTimer(&RefreshTimer);
}

Cullable::Result SpanningTreeUtilities::Cull()
{
	const TreeServer::ChildServers& children = TreeRoot->GetChildren();
	while (!children.empty())
	{
		TreeSocket* sock = children.front()->GetSocket();
		sock->Close();
	}

	for (const auto& [s, _] : timeoutlist)
		s->Close();

	TreeRoot->Cull();

	return Cullable::Cull();
}

SpanningTreeUtilities::~SpanningTreeUtilities()
{
	delete TreeRoot;
}

// Returns a list of DIRECT servers for a specific channel
void SpanningTreeUtilities::GetListOfServersForChannel(const Channel* c, TreeSocketSet& list, char status, const CUList& exempt_list) const
{
	ModeHandler::Rank minrank = 0;
	if (status)
	{
		PrefixMode* mh = ServerInstance->Modes.FindPrefix(status);
		if (mh)
			minrank = mh->GetPrefixRank();
	}

	TreeServer::ChildServers children = TreeRoot->GetChildren();
	for (const auto& [user, memb] : c->GetUsers())
	{
		if (IS_LOCAL(user))
			continue;

		if (minrank && memb->GetRank() < minrank)
			continue;

		if (exempt_list.find(user) == exempt_list.end())
		{
			TreeServer* best = TreeServer::Get(user);
			list.insert(best->GetSocket());

			TreeServer::ChildServers::iterator citer = std::find(children.begin(), children.end(), best);
			if (citer != children.end())
				children.erase(citer);
		}
	}

	// Check whether the servers which do not have users in the channel might need this message. This
	// is used to keep the chanhistory module synchronised between servers.
	for (const auto &[_, server] : Utils->serverlist)
	{
		if (!server->GetRoute())
			continue; // Local server

		ModResult result = Creator->routeeventprov.FirstResult(&ServerProtocol::RouteEventListener::OnRouteMessage, c, server);
		if (result == MOD_RES_ALLOW)
			list.insert(server->GetRoute()->GetSocket());
	}
}

void SpanningTreeUtilities::DoOneToAllButSender(const CmdBuilder& params, const TreeServer* omitroute) const
{
	const std::string& FullLine = params.str();

	for (const auto* Route : TreeRoot->GetChildren())
	{
		// Send the line if the route isn't the path to the one to be omitted
		if (Route != omitroute)
		{
			Route->GetSocket()->WriteLine(FullLine);
		}
	}
}

void SpanningTreeUtilities::DoOneToOne(const CmdBuilder& params, const Server* server)
{
	const TreeServer* ts = static_cast<const TreeServer*>(server);
	TreeSocket* sock = ts->GetSocket();
	if (sock)
		sock->WriteLine(params);
}

void SpanningTreeUtilities::RefreshIPCache()
{
	ValidIPs.clear();
	for (const auto& L : LinkBlocks)
	{
		bool isunix = L->IPAddr.find('/') != std::string::npos;
		if (!L->Port && !isunix)
		{
			ServerInstance->Logs.Warning(MODNAME, "Ignoring an IP link block without a port.");
			/* Invalid link block */
			continue;
		}

		ValidIPs.insert(ValidIPs.end(), L->AllowMasks.begin(), L->AllowMasks.end());

		irc::sockets::sockaddrs dummy(false);
		bool ipvalid = dummy.from_ip_port(L->IPAddr, L->Port);
		if ((L->IPAddr == "*") || (isunix) || (ipvalid))
			ValidIPs.push_back(L->IPAddr);
		else if (this->Creator->DNS)
		{
			auto* sr = new SecurityIPResolver(Creator, *this->Creator->DNS, L->IPAddr, L, DNS::QUERY_AAAA);
			try
			{
				this->Creator->DNS->Process(sr);
			}
			catch (const DNS::Exception&)
			{
				delete sr;
			}
		}
	}
}

void SpanningTreeUtilities::ReadConfiguration()
{
	const auto& options = ServerInstance->Config->ConfValue("options");
	AllowOptCommon = options->getBool("allowmismatch");
	AnnounceTSChange = options->getBool("announcets");
	PingWarnTime = options->getDuration("pingwarning", 15);
	PingFreq = options->getDuration("serverpingfreq", 60, 1);

	const auto& security = ServerInstance->Config->ConfValue("security");
	FlatLinks = security->getBool("flatlinks");
	HideServices = security->getBool("hideservices", security->getBool("hideulines"));
	HideSplits = security->getBool("hidesplits");

	const auto& performance = ServerInstance->Config->ConfValue("performance");
	quiet_bursts = performance->getBool("quietbursts");

	if (PingWarnTime >= PingFreq)
		PingWarnTime = 0;

	AutoconnectBlocks.clear();
	LinkBlocks.clear();

	for (const auto& [_, tag] : ServerInstance->Config->ConfTags("link"))
	{
		auto L = std::make_shared<Link>(tag);

		irc::spacesepstream sep = tag->getString("allowmask");
		for (std::string s; sep.GetToken(s);)
			L->AllowMasks.push_back(s);

		const std::string path = tag->getString("path");
		if (path.empty())
		{
			L->IPAddr = tag->getString("ipaddr");
			L->Port = tag->getNum<in_port_t>("port", 0);
			if (tag->getBool("sctp"))
			{
#ifdef IPPROTO_SCTP
				L->Protocol = IPPROTO_SCTP;
#else
				throw ModuleException((Module*)Creator, "Unable to use SCTP for outgoing connections as this platform does not support SCTP!");
#endif
			}
		}
		else
		{
			L->IPAddr = ServerInstance->Config->Paths.PrependData(path);
			L->Port = 0;
		}

		L->Name = tag->getString("name");
		L->SendPass = tag->getString("sendpass", tag->getString("password"));
		L->RecvPass = tag->getString("recvpass", tag->getString("password"));
		L->Fingerprint = tag->getString("fingerprint");
		L->HiddenFromStats = tag->getBool("statshidden");
		L->Timeout = tag->getDuration("timeout", 30);
		L->Hook = tag->getString("sslprofile");
		L->Bind = tag->getString("bind");
		L->Hidden = tag->getBool("hidden");

		if (!tag->getString("ssl").empty())
		{
			throw ModuleException((Module*)Creator, "Obsolete TLS configuration in link block at {}. See {}modules/spanningtree/#link for the correct way to configure TLS.",
				tag->source.str(), INSPIRCD_DOCS);
		}

		if (L->Name.empty())
			throw ModuleException((Module*)Creator, "Invalid configuration, found a link tag without a name!" + (!L->IPAddr.empty() ? " IP address: "+L->IPAddr : ""));

		if (L->Name.find('.') == std::string::npos)
			throw ModuleException((Module*)Creator, "The link name '"+L->Name+"' is invalid as it must contain at least one '.' character");

		if (L->Name.length() > ServerInstance->Config->Limits.MaxHost)
			throw ModuleException((Module*)Creator, "The link name '{}' is invalid as it is longer than {} characters", L->Name, ServerInstance->Config->Limits.MaxHost);

		if (L->RecvPass.empty())
			throw ModuleException((Module*)Creator, "Invalid configuration for server '"+L->Name+"', recvpass not defined");

		if (L->SendPass.empty())
			throw ModuleException((Module*)Creator, "Invalid configuration for server '"+L->Name+"', sendpass not defined");

		if ((L->SendPass.find(' ') != std::string::npos) || (L->RecvPass.find(' ') != std::string::npos))
			throw ModuleException((Module*)Creator, "Link block '" + L->Name + "' has a password set that contains a space character which is invalid");

		if ((L->SendPass[0] == ':') || (L->RecvPass[0] == ':'))
			throw ModuleException((Module*)Creator, "Link block '" + L->Name + "' has a password set that begins with a colon (:) which is invalid");

		if (L->IPAddr.empty())
		{
			L->IPAddr = "*";
			ServerInstance->Logs.Warning(MODNAME, "Configuration warning: Link block '" + L->Name + "' has no IP defined! This will allow any IP to connect as this server, and MAY not be what you want.");
		}

		if (!L->Port && L->IPAddr.find('/') == std::string::npos)
			ServerInstance->Logs.Warning(MODNAME, "Configuration warning: Link block '" + L->Name + "' has no port defined, you will not be able to /connect it.");

		std::transform(L->Fingerprint.begin(), L->Fingerprint.end(), L->Fingerprint.begin(), ::tolower);
		L->Fingerprint.erase(std::remove(L->Fingerprint.begin(), L->Fingerprint.end(), ':'), L->Fingerprint.end());

		LinkBlocks.push_back(L);
	}

	for (const auto& [_, tag] : ServerInstance->Config->ConfTags("autoconnect"))
	{
		auto A = std::make_shared<Autoconnect>(tag);
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
			throw ModuleException((Module*)Creator, "Invalid configuration for autoconnect, server cannot be empty!");
		}

		AutoconnectBlocks.push_back(A);
	}

	for (const auto& [_, server] : serverlist)
		server->CheckService();

	RefreshIPCache();
}

std::shared_ptr<Link> SpanningTreeUtilities::FindLink(const std::string& name)
{
	for (const auto& x : LinkBlocks)
	{
		if (InspIRCd::Match(x->Name, name, ascii_case_insensitive_map))
		{
			return x;
		}
	}
	return nullptr;
}

void SpanningTreeUtilities::SendChannelMessage(const User* source, const Channel* target, const std::string& text, char status, const ClientProtocol::TagMap& tags, const CUList& exempt_list, const char* message_type, const TreeSocket* omit) const
{
	CmdBuilder msg(source, message_type);
	msg.push_tags(tags);
	msg.push_raw(' ');
	if (status != 0)
		msg.push_raw(status);
	msg.push_raw(target->name);
	if (!text.empty())
		msg.push_last(text);

	TreeSocketSet list;
	this->GetListOfServersForChannel(target, list, status, exempt_list);

	for (auto* Sock : list)
	{
		if (Sock != omit)
			Sock->WriteLine(msg);
	}
}

std::string SpanningTreeUtilities::BuildLinkString(uint16_t proto, Module* mod)
{
	Module::LinkData data;
	std::string compatdata;
	mod->GetLinkData(data, compatdata);

	if (proto <= PROTO_INSPIRCD_3)
		return compatdata;

	std::stringstream buffer;
	for (Module::LinkData::const_iterator iter = data.begin(); iter != data.end(); ++iter)
	{
		if (iter != data.begin())
			buffer << '&';

		buffer << iter->first;
		if (!iter->second.empty())
			buffer << '=' << Percent::Encode(iter->second);
	}
	return buffer.str();
}

void SpanningTreeUtilities::SendListLimits(Channel* chan, TreeSocket* sock)
{
	std::stringstream buffer;
	for (auto* lm : ServerInstance->Modes.GetListModes())
		buffer << lm->GetModeChar() << " " << lm->GetLimit(chan) << " ";

	std::string bufferstr = buffer.str();
	if (bufferstr.empty())
		return; // Should never happen.

	bufferstr.pop_back();
	if (sock)
		sock->WriteLine(CommandMetadata::Builder(chan, "maxlist", bufferstr));
	else
		CommandMetadata::Builder(chan, "maxlist", bufferstr).Broadcast();
}
