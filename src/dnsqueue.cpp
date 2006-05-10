/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "inspircd_config.h"
#include "inspircd.h"
#include "inspircd_io.h"
#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <cstdio>
#include <time.h>
#include <string>
#ifdef GCC3
#include <ext/hash_map>
#else
#include <hash_map>
#endif
#include <map>
#include <sstream>
#include <vector>
#include <deque>
#include "users.h"
#include "globals.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include "dns.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;

class Lookup;

Lookup* dnslist[MAX_DESCRIPTORS];

//enum LookupState { reverse, forward };

class Lookup {
private:
	DNS resolver1;
	DNS resolver2;
	char u[NICKMAX];
	std::string hostname;
public:
	Lookup()
	{
		*u = 0;
		hostname = "";
	}

	void Reset()
	{
		*u = 0;
		hostname = "";
	}

	~Lookup()
	{
	}

	bool DoLookup(std::string nick)
	{
		hostname = "";
		userrec* usr = Find(nick);
		if (usr)
		{
			resolver1.SetNS(std::string(Config->DNSServer));
			if (!resolver1.ReverseLookup(std::string(usr->host)))
			{
				return false;
			}
			strlcpy(u,nick.c_str(),NICKMAX-1);

			/* ASSOCIATE WITH DNS LOOKUP LIST */
			if (resolver1.GetFD() != -1)
			{
				dnslist[resolver1.GetFD()] = this;
				return true;
			}
		}
		return false;
	}

	bool Done(int fdcheck)
	{
		if (hostname != "")
		{
			// doing forward lookup
			userrec* usr = NULL;
			if (resolver2.HasResult(fdcheck))
			{
				if (resolver2.GetFD() != -1)
				{
					dnslist[resolver2.GetFD()] = NULL;
					std::string ip = resolver2.GetResultIP();
					usr = Find(u);
					if (usr)
					{
						if (usr->registered > 3)
						{
							usr->dns_done = true;
							return true;
						}
						if ((hostname != "") && (usr->registered != 7))
						{
							if ((std::string((char*)inet_ntoa(usr->ip4)) == ip) && (hostname.length() < 65))
							{
								strlcpy(usr->host,hostname.c_str(),64);
								strlcpy(usr->dhost,hostname.c_str(),64);
								/*address_cache::iterator address = addrcache.find(usr->ip4);
								if (address == addrcache.end())
								{
									log(DEBUG,"Caching hostname %s -> %s",(char*)inet_ntoa(usr->ip4),hostname.c_str());
									addrcache[usr->ip4] = new std::string(hostname);
								}*/
								WriteServ(usr->fd,"NOTICE Auth :*** Found your hostname");
							}
							usr->dns_done = true;
							return true;
						}
					}
				}
				else
				{
					usr = Find(u);
					if (usr)
					{
						usr->dns_done = true;
					}
					return true;
				}
			}
			return false;
		}
		else
		{
			// doing reverse lookup
			userrec* usr = NULL;
			if (resolver1.HasResult(fdcheck))
			{
				usr = Find(u);
				if ((usr) && (usr->dns_done))
				{
					if (resolver1.GetFD() != -1)
						dnslist[resolver1.GetFD()] = NULL;
					return true;
				}
				if (resolver1.GetFD() != -1)
				{
					dnslist[resolver1.GetFD()] = NULL;
					hostname = resolver1.GetResult();
					if (usr)
					{
						if ((usr->registered > 3) || (hostname == ""))
						{
							WriteServ(usr->fd,"NOTICE Auth :*** Could not resolve your hostname -- Using your IP address instead");
							usr->dns_done = true;
							return true;
						}
					}
					if (hostname != "")
					{
						resolver2.ForwardLookup(hostname);
						if (resolver2.GetFD() != -1)
							dnslist[resolver2.GetFD()] = this;
					}
				}
			}
		}
		return false;
	}

	int GetFD()
	{
		userrec* usr = Find(u);
		if (!usr)
			return 0;
		if (usr->dns_done)
			return 0;
		return usr->fd;
	}
};

bool lookup_dns(const std::string &nick)
{
	/* First attempt to find the nickname */
	userrec* u = Find(nick);
	if (u)
	{
		/* Check the cache */
		/*address_cache::iterator address = addrcache.find(u->ip4);
		if (address != addrcache.end())
		{
			WriteServ(u->fd,"NOTICE Auth :*** Found your hostname (cached)");
			log(DEBUG,"Found cached host");
			strlcpy(u->host,address->second->c_str(),MAXBUF);
			strlcpy(u->dhost,address->second->c_str(),MAXBUF);
			u->dns_done = true;
			return true;
		}*/
		/* If the user exists, create a new
		 * lookup object, and associate it
		 * with the user. The lookup object
		 * will maintain the reference table
		 * which we use for quickly finding
		 * dns results. Please note that we
		 * do not associate a lookup with a
		 * userrec* pointer and we use the
		 * nickname instead because, by the
		 * time the DNS lookup has completed,
		 * the nickname could have quit and
		 * if we then try and access the
		 * pointer we get a nice segfault.
		 */
		Lookup* L = new Lookup();
		L->DoLookup(nick);
		return true;
	}
	return false;
}

void dns_poll(int fdcheck)
{
	/* Check the given file descriptor is in valid range */
	if ((fdcheck < 0) || (fdcheck > MAX_DESCRIPTORS))
		return;

	/* Try and find the file descriptor in our list of
	 * active DNS lookups
	 */
	Lookup *x = dnslist[fdcheck];
	if (x)
	{
		/* If it exists check if its a valid fd still */
		if (x->GetFD() != -1)
		{
			/* Check if its done, if it is delete it */
			if (x->Done(fdcheck))
			{
				/* We don't need to delete the file descriptor
				 * from the socket engine, as dns.cpp tracks it
				 * for us if we are in single-threaded country.
				 */
				delete x;
			}
		}
		else
		{
			/* its fd is dodgy, the dns code probably
			 * bashed it due to error. Free the class.
			 */
			delete x;
		}
		/* If we got down here, the dns lookup was valid, BUT,
		 * its still in progress. Be patient, and wait for
		 * more socketengine events to complete the lookups.
		 */
		return;
	}
	/* This FD doesnt belong here, lets be rid of it,
	 * just to be safe so we dont get any more events
	 * about it.
	 */
	if (ServerInstance && ServerInstance->SE)
		ServerInstance->SE->DelFd(fdcheck);
}

