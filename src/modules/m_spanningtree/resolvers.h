/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef M_SPANNINGTREE_RESOLVERS_H
#define M_SPANNINGTREE_RESOLVERS_H

#include "socket.h"
#include "inspircd.h"
#include "xline.h"

#include "utils.h"
#include "link.h"

/** Handle resolving of server IPs for the cache
 */
class SecurityIPResolver : public Resolver
{
 private:
	reference<Link> MyLink;
	SpanningTreeUtilities* Utils;
	Module* mine;
	std::string host;
	QueryType query;
 public:
	SecurityIPResolver(Module* me, SpanningTreeUtilities* U, const std::string &hostname, Link* x, bool &cached, QueryType qt)
		: Resolver(hostname, qt, cached, me), MyLink(x), Utils(U), mine(me), host(hostname), query(qt)
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
			SecurityIPResolver* res = new SecurityIPResolver(mine, Utils, host, MyLink, cached, DNS_QUERY_A);
			ServerInstance->AddResolver(res, cached);
			return;
		}
		ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Could not resolve IP associated with Link '%s': %s",
			MyLink->Name.c_str(),errormessage.c_str());
	}
};

/** This class is used to resolve server hostnames during /connect and autoconnect.
 * As of 1.1, the resolver system is seperated out from BufferedSocket, so we must do this
 * resolver step first ourselves if we need it. This is totally nonblocking, and will
 * callback to OnLookupComplete or OnError when completed. Once it has completed we
 * will have an IP address which we can then use to continue our connection.
 */
class ServernameResolver : public Resolver
{
 private:
	SpanningTreeUtilities* Utils;
	QueryType query;
	std::string host;
	reference<Link> MyLink;
	reference<Autoconnect> myautoconnect;
 public:
	ServernameResolver(SpanningTreeUtilities* Util, const std::string &hostname, Link* x, bool &cached, QueryType qt, Autoconnect* myac);
	void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached);
	void OnError(ResolverError e, const std::string &errormessage);
};

#endif
