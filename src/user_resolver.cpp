/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
UserResolver::UserResolver(LocalUser* user, std::string to_resolve, QueryType qt, bool &cache) :
	Resolver(to_resolve, qt, cache, NULL), uuid(user->uuid)
{
	this->fwd = (qt == DNS_QUERY_A || qt == DNS_QUERY_AAAA);
}

void UserResolver::OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
{
	UserResolver *res_forward; // for forward-resolution
	LocalUser* bound_user = (LocalUser*)ServerInstance->FindUUID(uuid);
	if (!bound_user)
	{
		ServerInstance->Logs->Log("RESOLVER", DEBUG, "Resolution finished for user '%s' who is gone", uuid.c_str());
		return;
	}

	ServerInstance->Logs->Log("RESOLVER", DEBUG, "DNS result for %s: '%s' -> '%s'", uuid.c_str(), input.c_str(), result.c_str());

	if (!fwd)
	{
		// first half of resolution is done. We now need to verify that the host matches.
		bound_user->stored_host = result;
		try
		{
			/* Check we didnt time out */
			if (bound_user->registered != REG_ALL)
			{
				bool lcached = false;
				if (bound_user->client_sa.sa.sa_family == AF_INET6)
				{
					/* IPV6 forward lookup */
					res_forward = new UserResolver(bound_user, result, DNS_QUERY_AAAA, lcached);
				}
				else
				{
					/* IPV4 lookup */
					res_forward = new UserResolver(bound_user, result, DNS_QUERY_A, lcached);
				}
				ServerInstance->AddResolver(res_forward, lcached);
			}
		}
		catch (CoreException& e)
		{
			ServerInstance->Logs->Log("RESOLVER", DEBUG,"Error in resolver: %s",e.GetReason());
		}
	}
	else
	{
		/* Both lookups completed */

		irc::sockets::sockaddrs* user_ip = &bound_user->client_sa;
		bool rev_match = false;
		if (user_ip->sa.sa_family == AF_INET6)
		{
			struct in6_addr res_bin;
			if (inet_pton(AF_INET6, result.c_str(), &res_bin))
			{
				rev_match = !memcmp(&user_ip->in6.sin6_addr, &res_bin, sizeof(res_bin));
			}
		}
		else
		{
			struct in_addr res_bin;
			if (inet_pton(AF_INET, result.c_str(), &res_bin))
			{
				rev_match = !memcmp(&user_ip->in4.sin_addr, &res_bin, sizeof(res_bin));
			}
		}
		
		if (rev_match)
		{
			std::string hostname = bound_user->stored_host;
			if (hostname.length() < 65)
			{
				/* Check we didnt time out */
				if ((bound_user->registered != REG_ALL) && (!bound_user->dns_done))
				{
					/* Hostnames starting with : are not a good thing (tm) */
					if (hostname[0] == ':')
						hostname.insert(0, "0");

					bound_user->WriteServ("NOTICE Auth :*** Found your hostname (%s)%s", hostname.c_str(), (cached ? " -- cached" : ""));
					bound_user->dns_done = true;
					bound_user->dhost.assign(hostname, 0, 64);
					bound_user->host.assign(hostname, 0, 64);
					/* Invalidate cache */
					bound_user->InvalidateCache();
				}
			}
			else
			{
				if (!bound_user->dns_done)
				{
					bound_user->WriteServ("NOTICE Auth :*** Your hostname is longer than the maximum of 64 characters, using your IP address (%s) instead.", bound_user->GetIPString());
					bound_user->dns_done = true;
				}
			}
		}
		else
		{
			if (!bound_user->dns_done)
			{
				bound_user->WriteServ("NOTICE Auth :*** Your hostname does not match up with your IP address. Sorry, using your IP address (%s) instead.", bound_user->GetIPString());
				bound_user->dns_done = true;
			}
		}

		// Save some memory by freeing this up; it's never used again in the user's lifetime.
		bound_user->stored_host.resize(0);
	}
}

void UserResolver::OnError(ResolverError e, const std::string &errormessage)
{
	LocalUser* bound_user = (LocalUser*)ServerInstance->FindUUID(uuid);
	if (bound_user)
	{
		bound_user->WriteServ("NOTICE Auth :*** Could not resolve your hostname: %s; using your IP address (%s) instead.", errormessage.c_str(), bound_user->GetIPString());
		bound_user->dns_done = true;
		bound_user->stored_host.resize(0);
		ServerInstance->stats->statsDnsBad++;
	}
}
