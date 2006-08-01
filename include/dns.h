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

struct dns_ip4list
{
	insp_inaddr ip;
	dns_ip4list *next;
};

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


/** The DNS class allows fast nonblocking resolution of hostnames
 * and ip addresses. It is based heavily upon firedns by Ian Gulliver.
 * Modules SHOULD avoid using this class to resolve hostnames and IP
 * addresses, as it is a low-level wrapper around the UDP socket routines
 * and is probably not abstracted enough for real use. Please see the
 * Resolver class if you wish to resolve hostnames.
 */
class DNS : public Extensible
{
 private:
	insp_inaddr *binip;
	unsigned char* result;
	unsigned char localbuf[1024];
	int t;
	int myid;
 public:
	int dns_getip4(const char* name);
	int dns_getname4(const insp_inaddr* ip);
	int dns_getresult();
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
 private:
	/**
	 * The lowlevel DNS object used by Resolver
	 */
	DNS Query;
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
	 * The file descriptor used for the DNS lookup
	 */
	int myid;
	/**
	 * The output data, e.g. a hostname or an IP.
	 */
	std::string result;
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
	 * @param dnsserver This optional parameter specifies an alterate nameserver to use.
	 * If it is not specified, or is an empty string, the value of ServerConfig::DNSServer
	 * is used instead.
	 * @throw ModuleException This class may throw an instance of ModuleException, in the
	 * event there are no more file descriptors, or a similar hard error occurs such as
	 * the network being down.
	 */
	Resolver(const std::string &source, bool forward, const std::string &dnsserver);
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
	bool ProcessResult();
	/**
	 * Returns the file descriptor of this class. This is primarily used by the core
	 * to determine where in various tables to place a pointer to your class, but it
	 * is safe to call and use this method.
	 */
	int GetId();
};

/**
 * Clear the pointer table used for Resolver classes
 */
void init_dns();
/**
 * Deal with a Resolver class which has become writeable
 */
void dns_deal_with_classes(int fd);
/**
 * Add a resolver class to our active table
 */
bool dns_add_class(Resolver* r);

#ifdef THREADED_DNS
/** This is the handler function for multi-threaded DNS.
 * It cannot be a class member as pthread will not let us
 * create a thread whos handler function is a member of
 * a class (ugh).
 */
void* dns_task(void* arg);
#endif

#endif
