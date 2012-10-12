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
#include "socket.h"
#include "xline.h"

#include "resolvers.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

/** This class is used to resolve server hostnames during /connect and autoconnect.
 * As of 1.1, the resolver system is seperated out from BufferedSocket, so we must do this
 * resolver step first ourselves if we need it. This is totally nonblocking, and will
 * callback to OnLookupComplete or OnError when completed. Once it has completed we
 * will have an IP address which we can then use to continue our connection.
 */
ServernameResolver::ServernameResolver(SpanningTreeUtilities* Util, const std::string &hostname, Link* x, bool &cached, QueryType qt, Autoconnect* myac)
	: Resolver(hostname, qt, cached, Util->Creator), Utils(Util), query(qt), host(hostname), MyLink(x), myautoconnect(myac)
{
}

void ServernameResolver::OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
{
	/* Initiate the connection, now that we have an IP to use.
	 * Passing a hostname directly to BufferedSocket causes it to
	 * just bail and set its FD to -1.
	 */
	TreeServer* CheckDupe = Utils->FindServer(MyLink->Name.c_str());
	if (!CheckDupe) /* Check that nobody tried to connect it successfully while we were resolving */
	{
		TreeSocket* newsocket = new TreeSocket(Utils, MyLink, myautoconnect, result);
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

void ServernameResolver::OnError(ResolverError e, const std::string &errormessage)
{
	/* Ooops! */
	if (query == DNS_QUERY_AAAA)
	{
		bool cached = false;
		ServernameResolver* snr = new ServernameResolver(Utils, host, MyLink, cached, DNS_QUERY_A, myautoconnect);
		ServerInstance->AddResolver(snr, cached);
		return;
	}
	ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: Unable to resolve hostname - %s", MyLink->Name.c_str(), errormessage.c_str() );
	Utils->Creator->ConnectServer(myautoconnect, false);
}

SecurityIPResolver::SecurityIPResolver(Module* me, SpanningTreeUtilities* U, const std::string &hostname, Link* x, bool &cached, QueryType qt)
		: Resolver(hostname, qt, cached, me), MyLink(x), Utils(U), mine(me), host(hostname), query(qt)
{
}

void SecurityIPResolver::OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
{
	for (std::vector<reference<Link> >::iterator i = Utils->LinkBlocks.begin(); i != Utils->LinkBlocks.end(); ++i)
	{
		Link* L = *i;
		if (L->IPAddr == host)
		{
			Utils->ValidIPs.push_back(result);
			break;
		}
	}
}

void SecurityIPResolver::OnError(ResolverError e, const std::string &errormessage)
{
	if (query == DNS_QUERY_AAAA)
	{
		bool cached = false;
		SecurityIPResolver* res = new SecurityIPResolver(mine, Utils, host, MyLink, cached, DNS_QUERY_A);
		ServerInstance->AddResolver(res, cached);
		return;
	}
	ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Could not resolve IP associated with Link '%s': %s",
		MyLink->Name.c_str(),errormessage.c_str());
}
