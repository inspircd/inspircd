/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2005-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


/*
dns.h - dns library very very loosely based on
firedns, Copyright (C) 2002 Ian Gulliver

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

#ifndef DNS_H
#define DNS_H

#include "socket.h"
#include "hashcomp.h"

/**
 * Query and resource record types
 */
enum QueryType
{
	/** Uninitialized Query */
	DNS_QUERY_NONE	= 0,
	/** 'A' record: an ipv4 address */
	DNS_QUERY_A	= 1,
	/** 'CNAME' record: An alias */
	DNS_QUERY_CNAME	= 5,
	/** 'PTR' record: a hostname */
	DNS_QUERY_PTR	= 12,
	/** 'AAAA' record: an ipv6 address */
	DNS_QUERY_AAAA	= 28,

	/** Force 'PTR' to use IPV4 scemantics */
	DNS_QUERY_PTR4	= 0xFFFD,
	/** Force 'PTR' to use IPV6 scemantics */
	DNS_QUERY_PTR6	= 0xFFFE
};

/**
 * Result status, used internally
 */
class CoreExport DNSResult
{
 public:
	/** Result ID
	 */
	int id;
	/** Result body, a hostname or IP address
	 */
	std::string result;
	/** Time-to-live value of the result
	 */
	unsigned long ttl;
	/** The original request, a hostname or IP address
	 */
	std::string original;
	/** The type of the request
	 */
	QueryType type;

	/** Build a DNS result.
	 * @param i The request ID
	 * @param res The request result, a hostname or IP
	 * @param timetolive The request time-to-live
	 * @param orig The original request, a hostname or IP
	 * @param qt The type of DNS query this result represents.
	 */
	DNSResult(int i, const std::string &res, unsigned long timetolive, const std::string &orig, QueryType qt = DNS_QUERY_NONE) : id(i), result(res), ttl(timetolive), original(orig), type(qt) { }
};

/**
 * Information on a completed lookup, used internally
 */
typedef std::pair<unsigned char*, std::string> DNSInfo;

/** Cached item stored in the query cache.
 */
class CoreExport CachedQuery
{
 public:
	/** The cached result data, an IP or hostname
	 */
	std::string data;
	/** The type of result this is
	 */
	QueryType type;
	/** The time when the item is due to expire
	 */
	time_t expires;

	/** Build a cached query
	 * @param res The result data, an IP or hostname
	 * @param qt The type of DNS query this instance represents.
	 * @param ttl The time-to-live value of the query result
	 */
	CachedQuery(const std::string &res, QueryType qt, unsigned int ttl);

	/** Returns the number of seconds remaining before this
	 * cache item has expired and should be removed.
	 */
	int CalcTTLRemaining();
};

/** DNS cache information. Holds IPs mapped to hostnames, and hostnames mapped to IPs.
 */
typedef nspace::hash_map<irc::string, CachedQuery, irc::hash> dnscache;

/**
 * Error types that class Resolver can emit to its error method.
 */
enum ResolverError
{
	RESOLVER_NOERROR	=	0,
	RESOLVER_NSDOWN		= 	1,
	RESOLVER_NXDOMAIN	=	2,
	RESOLVER_BADIP		=	3,
	RESOLVER_TIMEOUT	=	4,
	RESOLVER_FORCEUNLOAD	=	5
};

/**
 * Used internally to force PTR lookups to use a certain protocol scemantics,
 * e.g. x.x.x.x.in-addr.arpa for v4, and *.ip6.arpa for v6.
 */
enum ForceProtocol
{
	/** Forced to use ipv4 */
	PROTOCOL_IPV4 = 0,
	/** Forced to use ipv6 */
	PROTOCOL_IPV6 = 1
};

/**
 * The Resolver class is a high-level abstraction for resolving DNS entries.
 * It can do forward and reverse IPv4 lookups, and where IPv6 is supported, will
 * also be able to do those, transparent of protocols. Module developers must
 * extend this class via inheritence, and then insert a pointer to their derived
 * class into the core using Server::AddResolver(). Once you have done this,
 * the class will be able to receive callbacks. There are two callbacks which
 * can occur by calling virtual methods, one is a success situation, and the other
 * an error situation.
 */
class CoreExport Resolver
{
 protected:
	/**
	 * Pointer to creator module (if any, or NULL)
	 */
	ModuleRef Creator;
	/**
	 * The input data, either a host or an IP address
	 */
	std::string input;
	/**
	 * True if a forward lookup is being performed, false if otherwise
	 */
	QueryType querytype;
	/**
	 * The DNS erver being used for lookups. If this is an empty string,
	 * the value of ServerConfig::DNSServer is used instead.
	 */
	std::string server;
	/**
	 * The ID allocated to your lookup. This is a pseudo-random number
	 * between 0 and 65535, a value of -1 indicating a failure.
	 * The core uses this to route results to the correct objects.
	 */
	int myid;

	/**
	 * Cached result, if there is one
	 */
	CachedQuery *CQ;

	/**
	 * Time left before cache expiry
	 */
	int time_left;

 public:
	/**
	 * Initiate DNS lookup. Your class should not attempt to delete or free these
	 * objects, as the core will do this for you. They must always be created upon
	 * the heap using new, as you cannot be sure at what time they will be deleted.
	 * Allocating them on the stack or attempting to delete them yourself could cause
	 * the object to go 'out of scope' and cause a segfault in the core if the result
	 * arrives at a later time.
	 * @param source The IP or hostname to resolve
	 * @param qt The query type to perform. Resolution of 'A', 'AAAA', 'PTR' and 'CNAME' records
	 * is supported. Use one of the QueryType enum values to initiate this type of
	 * lookup. Resolution of 'AAAA' ipv6 records is always supported, regardless of
	 * wether InspIRCd is built with ipv6 support.
	 * To look up reverse records, specify one of DNS_QUERY_PTR4 or DNS_QUERY_PTR6 depending
	 * on the type of address you are looking up.
	 * @param cached The constructor will set this boolean to true or false depending
	 * on whether the DNS lookup you are attempting is cached (and not expired) or not.
	 * If the value is cached, upon return this will be set to true, otherwise it will
	 * be set to false. You should pass this value to InspIRCd::AddResolver(), which
	 * will then influence the behaviour of the method and determine whether a cached
	 * or non-cached result is obtained. The value in this variable is always correct
	 * for the given request when the constructor exits.
	 * @param creator See the note below.
	 * @throw ModuleException This class may throw an instance of ModuleException, in the
	 * event a lookup could not be allocated, or a similar hard error occurs such as
	 * the network being down. This will also be thrown if an invalid IP address is
	 * passed when resolving a 'PTR' record.
	 *
	 * NOTE: If you are instantiating your DNS lookup from a module, you should set the
	 * value of creator to point at your Module class. This way if your module is unloaded
	 * whilst lookups are in progress, they can be safely removed and your module will not
	 * crash the server.
	 */
	Resolver(const std::string &source, QueryType qt, bool &cached, Module* creator);

	/**
	 * The default destructor does nothing.
	 */
	virtual ~Resolver();

	/**
	 * When your lookup completes, this method will be called.
	 * @param result The resulting DNS lookup, either an IP address or a hostname.
	 * @param ttl The time-to-live value of the result, in the instance of a cached
	 * result, this is the number of seconds remaining before refresh/expiry.
	 * @param cached True if the result is a cached result, false if it was requested
	 * from the DNS server.
	 */
	virtual void OnLookupComplete(const std::string &result, unsigned int ttl, bool cached) = 0;

	/**
	 * If an error occurs (such as NXDOMAIN, no domain name found) then this method
	 * will be called.
	 * @param e A ResolverError enum containing the error type which has occured.
	 * @param errormessage The error text of the error that occured.
	 */
	virtual void OnError(ResolverError e, const std::string &errormessage);

	/**
	 * Returns the id value of this class. This is primarily used by the core
	 * to determine where in various tables to place a pointer to your class, but it
	 * is safe to call and use this method.
	 * As specified in RFC1035, each dns request has a 16 bit ID value, ranging
	 * from 0 to 65535. If there is an issue and the core cannot send your request,
	 * this method will return -1.
	 */
	int GetId();

	/**
	 * Returns the creator module, or NULL
	 */
	Module* GetCreator();

	/**
	 * If the result is a cached result, this triggers the objects
	 * OnLookupComplete. This is done because it is not safe to call
	 * the abstract virtual method from the constructor.
	 */
	void TriggerCachedResult();
};

/** DNS is a singleton class used by the core to dispatch dns
 * requests to the dns server, and route incoming dns replies
 * back to Resolver objects, based upon the request ID. You
 * should never use this class yourself.
 */
class CoreExport DNS : public EventHandler
{
 private:

	/**
	 * The maximum value of a dns request id,
	 * 16 bits wide, 0xFFFF.
	 */
	static const int MAX_REQUEST_ID = 0xFFFF;

	/** Maximum number of entries in cache
	 */
	static const unsigned int MAX_CACHE_SIZE = 1000;

	/**
	 * Currently cached items
	 */
	dnscache* cache;

	/** A timer which ticks every hour to remove expired
	 * items from the DNS cache.
	 */
	class CacheTimer* PruneTimer;

	/**
	 * Build a dns packet payload
	 */
	int MakePayload(const char* name, const QueryType rr, const unsigned short rr_class, unsigned char* payload);

 public:

	irc::sockets::sockaddrs myserver;

	/**
	 * Currently active Resolver classes
	 */
	Resolver* Classes[MAX_REQUEST_ID];

	/**
	 * Requests that are currently 'in flight'
	 */
	DNSRequest* requests[MAX_REQUEST_ID];

	/**
	 * The port number DNS requests are made on,
	 * and replies have as a source-port number.
	 */
	static const int QUERY_PORT = 53;

	/**
	 * Fill an rr (resource record) with data from input
	 */
	static void FillResourceRecord(ResourceRecord* rr, const unsigned char* input);

	/**
	 * Fill a header with data from input limited by a length
	 */
	static void FillHeader(DNSHeader *header, const unsigned char *input, const int length);

	/**
	 * Empty out a header into a data stream ready for transmission "on the wire"
	 */
	static void EmptyHeader(unsigned char *output, const DNSHeader *header, const int length);

	/**
	 * Start the lookup of an ipv4 from a hostname
	 */
	int GetIP(const char* name);

	/**
	 * Start lookup of a hostname from an ip, but
	 * force a specific protocol to be used for the lookup
	 * for example to perform an ipv6 reverse lookup.
	 */
	int GetNameForce(const char *ip, ForceProtocol fp);

	/**
	 * Start lookup of an ipv6 from a hostname
	 */
	int GetIP6(const char *name);

	/**
	 * Start lookup of a CNAME from another hostname
	 */
	int GetCName(const char* alias);

	/**
	 * Fetch the result string (an ip or host)
	 * and/or an error message to go with it.
	 */
	DNSResult GetResult();

	/**
	 * Handle a SocketEngine read event
	 * Inherited from EventHandler
	 */
	void HandleEvent(EventType et, int errornum = 0);

	/**
	 * Add a Resolver* to the list of active classes
	 */
	bool AddResolverClass(Resolver* r);

	/**
	 * Add a query to the list to be sent
	 */
	DNSRequest* AddQuery(DNSHeader *header, int &id, const char* original);

	/**
	 * The constructor initialises the dns socket,
	 * and clears the request lists.
	 */
	DNS();

	/**
	 * Re-initialize the DNS subsystem.
	 */
	void Rehash();

	/**
	 * Destructor
	 */
	~DNS();

	/**
	 * Turn an in6_addr into a .ip6.arpa domain
	 */
	static void MakeIP6Int(char* query, const in6_addr *ip);

	/**
	 * Clean out all dns resolvers owned by a particular
	 * module, to make unloading a module safe if there
	 * are dns requests currently in progress.
	 */
	void CleanResolvers(Module* module);

	/** Return the cached value of an IP or hostname
	 * @param source An IP or hostname to find in the cache.
	 * @return A pointer to a CachedQuery if the item exists,
	 * otherwise NULL.
	 */
	CachedQuery* GetCache(const std::string &source);

	/** Delete a cached item from the DNS cache.
	 * @param source An IP or hostname to remove
	 */
	void DelCache(const std::string &source);

	/** Clear all items from the DNS cache immediately.
	 */
	int ClearCache();

	/** Prune the DNS cache, e.g. remove all expired
	 * items and rehash the cache buckets, but leave
	 * items in the hash which are still valid.
	 */
	int PruneCache();
};

#endif

