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

#ifndef __RESOLVERS__H__
#define __RESOLVERS__H__

#include "configreader.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "inspircd.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/utils.h"
#include "m_spanningtree/link.h"

/** Handle resolving of server IPs for the cache
 */
class SecurityIPResolver : public Resolver
{
 private:
	Link MyLink;
	SpanningTreeUtilities* Utils;
	Module* mine;
	std::string host;
	QueryType query;
 public:
	SecurityIPResolver(Module* me, SpanningTreeUtilities* U, InspIRCd* Instance, const std::string &hostname, Link x, bool &cached, QueryType qt)
		: Resolver(Instance, hostname, qt, cached, me), MyLink(x), Utils(U), mine(me), host(hostname), query(qt)
	{
	}

	void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
	{
		Utils->ValidIPs.push_back(result);
	}

	void OnError(ResolverError e, const std::string &errormessage)
	{
		if (query == DNS_QUERY_AAAA)
		{
			bool cached;
			SecurityIPResolver* res = new SecurityIPResolver(mine, Utils, ServerInstance, host, MyLink, cached, DNS_QUERY_A);
			ServerInstance->AddResolver(res, cached);
			return;
		}
		ServerInstance->Log(DEFAULT,"Could not resolve IP associated with Link '%s': %s",MyLink.Name.c_str(),errormessage.c_str());
	}
};

/** This class is used to resolve server hostnames during /connect and autoconnect.
 * As of 1.1, the resolver system is seperated out from InspSocket, so we must do this
 * resolver step first ourselves if we need it. This is totally nonblocking, and will
 * callback to OnLookupComplete or OnError when completed. Once it has completed we
 * will have an IP address which we can then use to continue our connection.
 */
class ServernameResolver : public Resolver
{
 private:
        /** A copy of the Link tag info for what we're connecting to.
         * We take a copy, rather than using a pointer, just in case the
         * admin takes the tag away and rehashes while the domain is resolving.
         */
        Link MyLink;
        SpanningTreeUtilities* Utils;
	QueryType query;
	std::string host;
	Module* mine;
 public:
        ServernameResolver(Module* me, SpanningTreeUtilities* Util, InspIRCd* Instance, const std::string &hostname, Link x, bool &cached, QueryType qt);
        void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached);
        void OnError(ResolverError e, const std::string &errormessage);
};

#endif
