/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/** This class is used to resolve server hostnames during /connect and autoconnect.
 * As of 1.1, the resolver system is seperated out from BufferedSocket, so we must do this
 * resolver step first ourselves if we need it. This is totally nonblocking, and will
 * callback to OnLookupComplete or OnError when completed. Once it has completed we
 * will have an IP address which we can then use to continue our connection.
 */
ServernameResolver::ServernameResolver(DNS::Manager* mgr, const std::string& hostname, Link* x, DNS::QueryType qt, Autoconnect* myac)
	: DNS::Request(mgr, Utils->Creator, hostname, qt)
	, query(qt), host(hostname), MyLink(x), myautoconnect(myac)
{
}

void ServernameResolver::OnLookupComplete(const DNS::Query *r)
{
	const DNS::ResourceRecord &ans_record = r->answers[0];

	/* Initiate the connection, now that we have an IP to use.
	 * Passing a hostname directly to BufferedSocket causes it to
	 * just bail and set its FD to -1.
	 */
	TreeServer* CheckDupe = Utils->FindServer(MyLink->Name.c_str());
	if (!CheckDupe) /* Check that nobody tried to connect it successfully while we were resolving */
	{
		TreeSocket* newsocket = new TreeSocket(MyLink, myautoconnect, ans_record.rdata);
		if (newsocket->GetFd() > -1)
		{
			/* We're all OK */
		}
		else
		{
			/* Something barfed, show the opers */
			ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: %s.",
				MyLink->Name.c_str(), newsocket->getError().c_str());
			ServerInstance->GlobalCulls.AddItem(newsocket);
		}
	}
}

void ServernameResolver::OnError(const DNS::Query *r)
{
	/* Ooops! */
	if (query == DNS::QUERY_AAAA)
	{
		ServernameResolver* snr = new ServernameResolver(this->manager, host, MyLink, DNS::QUERY_A, myautoconnect);
		try
		{
			this->manager->Process(snr);
			return;
		}
		catch (DNS::Exception &)
		{
			delete snr;
		}
	}

	ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: Unable to resolve hostname - %s", MyLink->Name.c_str(), this->manager->GetErrorStr(r->error).c_str());
	Utils->Creator->ConnectServer(myautoconnect, false);
}

SecurityIPResolver::SecurityIPResolver(Module* me, DNS::Manager* mgr, const std::string& hostname, Link* x, DNS::QueryType qt)
	: DNS::Request(mgr, me, hostname, qt)
	, MyLink(x), mine(me), host(hostname), query(qt)
{
}

void SecurityIPResolver::OnLookupComplete(const DNS::Query *r)
{
	const DNS::ResourceRecord &ans_record = r->answers[0];

	for (std::vector<reference<Link> >::iterator i = Utils->LinkBlocks.begin(); i != Utils->LinkBlocks.end(); ++i)
	{
		Link* L = *i;
		if (L->IPAddr == host)
		{
			Utils->ValidIPs.push_back(ans_record.rdata);
			break;
		}
	}
}

void SecurityIPResolver::OnError(const DNS::Query *r)
{
	if (query == DNS::QUERY_AAAA)
	{
		SecurityIPResolver* res = new SecurityIPResolver(mine, this->manager, host, MyLink, DNS::QUERY_A);
		try
		{
			this->manager->Process(res);
			return;
		}
		catch (DNS::Exception &)
		{
			delete res;
		}
	}
	ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Could not resolve IP associated with Link '%s': %s",
		MyLink->Name.c_str(), this->manager->GetErrorStr(r->error).c_str());
}

CacheRefreshTimer::CacheRefreshTimer()
	: Timer(3600, true)
{
}

bool CacheRefreshTimer::Tick(time_t TIME)
{
	Utils->RefreshIPCache();
	return true;
}
