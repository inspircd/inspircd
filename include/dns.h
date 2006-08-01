/*
dns.h - dns library declarations based on firedns Copyright (C) 2002 Ian Gulliver

This program is free software; you can redistribute it and/or modify
it under the terms of version 2 of the GNU General Public License as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _DNS_H
#define _DNS_H

#include <string>
#include "inspircd_config.h"
#include "socket.h"
#include "base.h"

typedef std::pair<int,std::string> DNSResult;

/**
 * Error types that class Resolver can emit to its error method.
 */
enum ResolverError
{
	RESOLVER_NOERROR	=	0,
	RESOLVER_NSDOWN		= 	1,
	RESOLVER_NXDOMAIN	=	2,
	RESOLVER_NOTREADY	=	3
};


/** DNS is a singleton class used by the core to dispatch dns
 * requests to the dns server, and route incoming dns replies
 * back to Resolver objects, based upon the request ID. You
 * should never use this class yourself.
 */
class DNS : public Extensible
{
 private:
	int t;
	int myid;
 public:
	int dns_getip4(const char* name);
	int dns_getname4(const insp_inaddr* ip);
	DNSResult dns_getresult();
	DNS();
	~DNS();
};

/**
 * The Resolver class is a high-level abstraction for resolving DNS entries.
 * It can do forward and reverse IPv4 lookups, and when IPv6 is supported, will
 * also be able to do those, transparent of protocols. Module developers must
 * extend this class via inheritence, and then insert a pointer to their derived
 * class into the core using Server::AddResolver(). Once you have done this,
 * the class will be able to receive callbacks. There are two callbacks which
 * can occur by calling virtual methods, one is a success situation, and the other
 * an error situation.
 */
class Resolver : public Extensible
{
 protected:
	/**
	 * The input data, either a host or an IP address
	 */
	std::string input;
	/**
	 * True if a forward lookup is being performed, false if otherwise
	 */
	bool fwd;
	/**
	 * The DNS erver being used for lookups. If this is an empty string,
	 * the value of ServerConfig::DNSServer is used instead.
	 */
	std::string server;
	/**
	 * The ID allocated to your lookup. This is a pseud-random number
	 * between 0 and 65535, a value of -1 indicating a failure.
	 * The core uses this to route results to the correct objects.
	 */
	int myid;
 public:
	/**
	 * Initiate DNS lookup. Your class should not attempt to delete or free these
	 * objects, as the core will do this for you. They must always be created upon
	 * the heap using new, as you cannot be sure at what time they will be deleted.
	 * Allocating them on the stack or attempting to delete them yourself could cause
	 * the object to go 'out of scope' and cause a segfault in the core if the result
	 * arrives at a later time.
	 * @param source The IP or hostname to resolve
	 * @param forward Set to true to perform a forward lookup (hostname to ip) or false
	 * to perform a reverse lookup (ip to hostname). Lookups on A records and PTR
	 * records are supported. CNAME and MX are not supported by this resolver.
	 * If InspIRCd is compiled with ipv6 support, lookups on AAAA records are preferred
	 * and supported over A records.
	 * @throw ModuleException This class may throw an instance of ModuleException, in the
	 * event a lookup could not be allocated, or a similar hard error occurs such as
	 * the network being down.
	 */
	Resolver(const std::string &source, bool forward);
	/**
	 * The default destructor does nothing.
	 */
	virtual ~Resolver();
	/**
	 * When your lookup completes, this method will be called.
	 * @param result The resulting DNS lookup, either an IP address or a hostname.
	 */
	virtual void OnLookupComplete(const std::string &result);
	/**
	 * If an error occurs (such as NXDOMAIN, no domain name found) then this method
	 * will be called.
	 * @param e A ResolverError enum containing the error type which has occured.
	 */
	virtual void OnError(ResolverError e);
	/**
	 * This method is called by the core when the object's file descriptor is ready
	 * for reading, and will then dispatch a call to either OnLookupComplete or
	 * OnError. You should never call this method yourself.
	 */
	bool ProcessResult(const std::string &result);
	/**
	 * Returns the file descriptor of this class. This is primarily used by the core
	 * to determine where in various tables to place a pointer to your class, but it
	 * is safe to call and use this method.
	 */
	int GetId();
};

/**
 * Clear the pointer table used for Resolver classes,
 * translate ServerConfig::DNSServer into an insp_inaddr,
 * establish binding on UDP socket for DNS requests.
 */
void init_dns();
/**
 * Deal with a Resolver class which has become readable
 */
void dns_deal_with_classes(int fd);
/**
 * Add a resolver class to our active table
 */
bool dns_add_class(Resolver* r);

#endif
