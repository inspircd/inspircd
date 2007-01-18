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

/** Handle resolving of server IPs for the cache
 */
class SecurityIPResolver : public Resolver
{
 private:
	Link MyLink;
	SpanningTreeUtilities* Utils;
 public:
	SecurityIPResolver(Module* me, SpanningTreeUtilities* U, InspIRCd* Instance, const std::string &hostname, Link x, bool &cached)
		: Resolver(Instance, hostname, DNS_QUERY_FORWARD, cached, me), MyLink(x), Utils(U)
	{
	}

	void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached)
	{
		Utils->ValidIPs.push_back(result);
	}

	void OnError(ResolverError e, const std::string &errormessage)
	{
		ServerInstance->Log(DEFAULT,"Could not resolve IP associated with Link '%s': %s",MyLink.Name.c_str(),errormessage.c_str());
	}
};

#endif
