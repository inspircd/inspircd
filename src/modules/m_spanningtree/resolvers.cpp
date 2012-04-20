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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

/** This class is used to resolve server hostnames during /connect and autoconnect.
 * As of 1.1, the resolver system is seperated out from BufferedSocket, so we must do this
 * resolver step first ourselves if we need it. This is totally nonblocking, and will
 * callback to OnLookupComplete or OnError when completed. Once it has completed we
 * will have an IP address which we can then use to continue our connection.
 */
ServernameResolver::ServernameResolver(Module* me, SpanningTreeUtilities* Util, InspIRCd* Instance, const std::string &hostname, Link x, bool &cached, QueryType qt) : Resolver(Instance, hostname, qt, cached, me), MyLink(x), Utils(Util), query(qt), host(hostname), mine(me)
{
	/* Nothing in here, folks */
}

void ServernameResolver::OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
{
	/* Initiate the connection, now that we have an IP to use.
	 * Passing a hostname directly to BufferedSocket causes it to
	 * just bail and set its FD to -1.
	 */
	TreeServer* CheckDupe = Utils->FindServer(MyLink.Name.c_str());
	if (!CheckDupe) /* Check that nobody tried to connect it successfully while we were resolving */
	{

		if ((!MyLink.Hook.empty()) && (Utils->hooks.find(MyLink.Hook.c_str()) ==  Utils->hooks.end()))
			return;

		TreeSocket* newsocket = new TreeSocket(this->Utils, ServerInstance, result,MyLink.Port,MyLink.Timeout ? MyLink.Timeout : 10,MyLink.Name.c_str(),
							MyLink.Bind, MyLink.Hook.empty() ? NULL : Utils->hooks[MyLink.Hook.c_str()]);
		if (newsocket->GetFd() > -1)
		{
			/* We're all OK */
		}
		else
		{
			/* Something barfed, show the opers */
			ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: %s.",MyLink.Name.c_str(),strerror(errno));
			if (ServerInstance->SocketCull.find(newsocket) == ServerInstance->SocketCull.end())
				ServerInstance->SocketCull[newsocket] = newsocket;
			Utils->DoFailOver(&MyLink);
		}
	}
}

void ServernameResolver::OnError(ResolverError e, const std::string &errormessage)
{
	/* Ooops! */
	if (query == DNS_QUERY_AAAA)
	{
		bool cached;
		ServernameResolver* snr = new ServernameResolver(mine, Utils, ServerInstance, host, MyLink, cached, DNS_QUERY_A);
		ServerInstance->AddResolver(snr, cached);
		return;
	}
	ServerInstance->SNO->WriteToSnoMask('l', "CONNECT: Error connecting \002%s\002: Unable to resolve hostname - %s", MyLink.Name.c_str(), errormessage.c_str() );
	Utils->DoFailOver(&MyLink);
}

