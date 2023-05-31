/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2016 Adam <Adam@anope.org>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

#include "cachetimer.h"
#include "resolvers.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"

ServerNameResolver::ServerNameResolver(DNS::Manager* mgr, const std::string& hostname, const std::shared_ptr<Link>& l, DNS::QueryType qt, const std::shared_ptr<Autoconnect>& a)
	: DNS::Request(mgr, Utils->Creator, hostname, qt)
	, autoconnect(a)
	, link(l)
{
}

void ServerNameResolver::OnLookupComplete(const DNS::Query* r)
{
	const DNS::ResourceRecord* const ans_record = r->FindAnswerOfType(this->question.type);
	if (!ans_record)
	{
		OnError(r);
		return;
	}

	irc::sockets::sockaddrs sa(false);
	if (!sa.from_ip_port(ans_record->rdata, link->Port))
	{
		// We had a result but it wasn't a valid IPv4/IPv6.
		OnError(r);
		return;
	}

	/* Initiate the connection, now that we have an IP to use.
	 * Passing a hostname directly to BufferedSocket causes it to
	 * just bail and set its FD to -1.
	 */
	TreeServer* CheckDupe = Utils->FindServer(link->Name);
	if (!CheckDupe) /* Check that nobody tried to connect it successfully while we were resolving */
	{
		auto* newsocket = new TreeSocket(link, autoconnect, sa);
		if (!newsocket->HasFd())
		{
			/* Something barfed, show the opers */
			ServerInstance->SNO.WriteToSnoMask('l', "CONNECT: Error connecting \002{}\002: {}.",
				link->Name, newsocket->GetError());
			ServerInstance->GlobalCulls.AddItem(newsocket);
		}
	}
}

void ServerNameResolver::OnError(const DNS::Query* r)
{
	if (r->error == DNS::ERROR_UNLOADED)
	{
		// We're being unloaded, skip the snotice and ConnectServer() below to prevent autoconnect creating new sockets
		return;
	}

	if (question.type == DNS::QUERY_AAAA)
	{
		auto* snr = new ServerNameResolver(this->manager, question.name, link, DNS::QUERY_A, autoconnect);
		try
		{
			this->manager->Process(snr);
			return;
		}
		catch (const DNS::Exception&)
		{
			delete snr;
		}
	}

	ServerInstance->SNO.WriteToSnoMask('l', "CONNECT: Error connecting \002{}\002: Unable to resolve hostname - {}",
		link->Name, this->manager->GetErrorStr(r->error));
	Utils->Creator->ConnectServer(autoconnect, false);
}

SecurityIPResolver::SecurityIPResolver(Module* me, DNS::Manager* mgr, const std::string& hostname, const std::shared_ptr<Link>& l, DNS::QueryType qt)
	: DNS::Request(mgr, me, hostname, qt)
	, link(l)
{
}

bool SecurityIPResolver::CheckIPv4()
{
	// We only check IPv4 addresses if we have checked IPv6.
	if (question.type != DNS::QUERY_AAAA)
		return false;

	auto* res = new SecurityIPResolver(creator, manager, question.name, link, DNS::QUERY_A);
	try
	{
		this->manager->Process(res);
		return true;
	}
	catch (const DNS::Exception&)
	{
		delete res;
		return false;
	}
}

void SecurityIPResolver::OnLookupComplete(const DNS::Query* r)
{
	for (const auto& L : Utils->LinkBlocks)
	{
		if (L->IPAddr == question.name)
		{
			for (const auto& ans_record : r->answers)
			{
				if (ans_record.type != this->question.type)
					continue;

				Utils->ValidIPs.push_back(ans_record.rdata);
				ServerInstance->Logs.Normal(MODNAME, "Resolved '{}' as a valid IP address for link '{}'",
					ans_record.rdata, link->Name);
			}
			break;
		}
	}

	CheckIPv4();
}

void SecurityIPResolver::OnError(const DNS::Query* r)
{
	// This can be called because of us being unloaded but we don't have to do anything differently
	if (CheckIPv4())
		return;

	ServerInstance->Logs.Warning(MODNAME, "Could not resolve IP associated with link '{}': {}",
		link->Name, this->manager->GetErrorStr(r->error));
}

CacheRefreshTimer::CacheRefreshTimer()
	: Timer(3600, true)
{
}

bool CacheRefreshTimer::Tick()
{
	Utils->RefreshIPCache();
	return true;
}
