/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
UserResolver::UserResolver(InspIRCd* Instance, User* user, std::string to_resolve, QueryType qt, bool &cache) :
	Resolver(Instance, to_resolve, qt, cache), bound_user(user)
{
	this->fwd = (qt == DNS_QUERY_A || qt == DNS_QUERY_AAAA);
	this->bound_fd = user->GetFd();
}

void UserResolver::OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
{
	UserResolver *res_forward; // for forward-resolution

	if ((!this->fwd) && (ServerInstance->SE->GetRef(this->bound_fd) == this->bound_user))
	{
		this->bound_user->stored_host = result;
		try
		{
			/* Check we didnt time out */
			if (this->bound_user->registered != REG_ALL)
			{
				bool lcached = false;
#ifdef IPV6
				if (this->bound_user->GetProtocolFamily() == AF_INET6)
				{
					/* IPV6 forward lookup (with possibility of 4in6) */
					const char* ip = this->bound_user->GetIPString();
					res_forward = new UserResolver(this->ServerInstance, this->bound_user, result, (!strncmp(ip, "0::ffff:", 8) ? DNS_QUERY_A : DNS_QUERY_AAAA), lcached);
				}
				else
					/* IPV4 lookup (mixed protocol mode) */
#endif
				{
					/* IPV4 lookup (ipv4 only mode) */
					res_forward = new UserResolver(this->ServerInstance, this->bound_user, result, DNS_QUERY_A, lcached);
				}
				this->ServerInstance->AddResolver(res_forward, lcached);
			}
		}
		catch (CoreException& e)
		{
			ServerInstance->Logs->Log("RESOLVER", DEBUG,"Error in resolver: %s",e.GetReason());
		}
	}
	else if ((this->fwd) && (ServerInstance->SE->GetRef(this->bound_fd) == this->bound_user))
	{
		/* Both lookups completed */

		sockaddr* user_ip = this->bound_user->ip;
		bool rev_match = false;
#ifdef IPV6
		if (user_ip->sa_family == AF_INET6)
		{
			struct in6_addr res_bin;
			if (inet_pton(AF_INET6, result.c_str(), &res_bin))
			{
				sockaddr_in6* user_ip6 = reinterpret_cast<sockaddr_in6*>(user_ip);
				rev_match = !memcmp(&user_ip6->sin6_addr, &res_bin, sizeof(res_bin));
			}
		}
		else
#endif
		{
			struct in_addr res_bin;
			if (inet_pton(AF_INET, result.c_str(), &res_bin))
			{
				sockaddr_in* user_ip4 = reinterpret_cast<sockaddr_in*>(user_ip);
				rev_match = !memcmp(&user_ip4->sin_addr, &res_bin, sizeof(res_bin));
			}
		}
		
		if (rev_match)
		{
			std::string hostname = this->bound_user->stored_host;
			if (hostname.length() < 65)
			{
				/* Check we didnt time out */
				if ((this->bound_user->registered != REG_ALL) && (!this->bound_user->dns_done))
				{
					/* Hostnames starting with : are not a good thing (tm) */
					if (hostname[0] == ':')
						hostname.insert(0, "0");

					this->bound_user->WriteServ("NOTICE Auth :*** Found your hostname (%s)%s", hostname.c_str(), (cached ? " -- cached" : ""));
					this->bound_user->dns_done = true;
					this->bound_user->dhost.assign(hostname, 0, 64);
					this->bound_user->host.assign(hostname, 0, 64);
					/* Invalidate cache */
					this->bound_user->InvalidateCache();
				}
			}
			else
			{
				if (!this->bound_user->dns_done)
				{
					this->bound_user->WriteServ("NOTICE Auth :*** Your hostname is longer than the maximum of 64 characters, using your IP address (%s) instead.", this->bound_user->GetIPString());
					this->bound_user->dns_done = true;
				}
			}
		}
		else
		{
			if (!this->bound_user->dns_done)
			{
				this->bound_user->WriteServ("NOTICE Auth :*** Your hostname does not match up with your IP address. Sorry, using your IP address (%s) instead.", this->bound_user->GetIPString());
				this->bound_user->dns_done = true;
			}
		}

		// Save some memory by freeing this up; it's never used again in the user's lifetime.
		this->bound_user->stored_host.resize(0);
	}
}

void UserResolver::OnError(ResolverError e, const std::string &errormessage)
{
	if (ServerInstance->SE->GetRef(this->bound_fd) == this->bound_user)
	{
		this->bound_user->WriteServ("NOTICE Auth :*** Could not resolve your hostname: %s; using your IP address (%s) instead.", errormessage.c_str(), this->bound_user->GetIPString());
		this->bound_user->dns_done = true;
		this->bound_user->stored_host.resize(0);
		ServerInstance->stats->statsDnsBad++;
	}
}
