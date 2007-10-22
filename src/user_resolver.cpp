/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDuserresolver */

#include "inspircd.h"

UserResolver::UserResolver(InspIRCd* Instance, User* user, std::string to_resolve, QueryType qt, bool &cache) :
	Resolver(Instance, to_resolve, qt, cache), bound_user(user)
{
	this->fwd = (qt == DNS_QUERY_A || qt == DNS_QUERY_AAAA);
	this->bound_fd = user->GetFd();
}

void UserResolver::OnLookupComplete(const std::string &result, unsigned int ttl, bool cached, int resultnum)
{
	/* We are only interested in the first matching result */
	if (resultnum)
		return;

	if ((!this->fwd) && (ServerInstance->SE->GetRef(this->bound_fd) == this->bound_user))
	{
		this->bound_user->stored_host = result;
		try
		{
			/* Check we didnt time out */
			if (this->bound_user->registered != REG_ALL)
			{
				bool cached;
#ifdef IPV6
				if (this->bound_user->GetProtocolFamily() == AF_INET6)
				{
					/* IPV6 forward lookup (with possibility of 4in6) */
					const char* ip = this->bound_user->GetIPString();
					bound_user->res_forward = new UserResolver(this->ServerInstance, this->bound_user, result, (!strncmp(ip, "0::ffff:", 8) ? DNS_QUERY_A : DNS_QUERY_AAAA), cached);
				}
				else
					/* IPV4 lookup (mixed protocol mode) */
#endif
				/* IPV4 lookup (ipv4 only mode) */
				bound_user->res_forward = new UserResolver(this->ServerInstance, this->bound_user, result, DNS_QUERY_A, cached);
				this->ServerInstance->AddResolver(bound_user->res_forward, cached);
			}
		}
		catch (CoreException& e)
		{
			ServerInstance->Log(DEBUG,"Error in resolver: %s",e.GetReason());
		}
	}
	else if ((this->fwd) && (ServerInstance->SE->GetRef(this->bound_fd) == this->bound_user))
	{
		/* Both lookups completed */
		std::string result2("0::ffff:");
		result2.append(result);
		if (this->bound_user->GetIPString() == result || this->bound_user->GetIPString() == result2)
		{
			std::string hostname = this->bound_user->stored_host;
			if (hostname.length() < 65)
			{
				/* Check we didnt time out */
				if ((this->bound_user->registered != REG_ALL) && (!this->bound_user->dns_done))
				{
					/* Hostnames starting with : are not a good thing (tm) */
					if (*(hostname.c_str()) == ':')
						hostname.insert(0, "0");

					this->bound_user->WriteServ("NOTICE Auth :*** Found your hostname (%s)%s", hostname.c_str(), (cached ? " -- cached" : ""));
					this->bound_user->dns_done = true;
					strlcpy(this->bound_user->dhost, hostname.c_str(),64);
					strlcpy(this->bound_user->host, hostname.c_str(),64);
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
	}
}

void UserResolver::OnError(ResolverError e, const std::string &errormessage)
{
	if (ServerInstance->SE->GetRef(this->bound_fd) == this->bound_user)
	{
		this->bound_user->WriteServ("NOTICE Auth :*** Could not resolve your hostname: %s; using your IP address (%s) instead.", errormessage.c_str(), this->bound_user->GetIPString());
		this->bound_user->dns_done = true;
		ServerInstance->stats->statsDnsBad++;
	}
}


