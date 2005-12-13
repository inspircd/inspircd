/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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
#include "inspircd_util.h"
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
#include <sys/poll.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include "dns.h"
#include "helperfuncs.h"
#include "hashcomp.h"
#include "socketengine.h"

extern SocketEngine* SE;
typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, irc::StrHashComp> user_hash;
extern user_hash clientlist;
extern char DNSServer[MAXBUF];

class Lookup;

Lookup* dnslist[65535];

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
		strcpy(u,"");
	}

	void Reset()
	{
		strcpy(u,"");
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
			resolver1.SetNS(std::string(DNSServer));
			if (!resolver1.ReverseLookup(std::string(usr->host)))
			{
				return false;
			}
			strlcpy(u,nick.c_str(),NICKMAX);

			/* ASSOCIATE WITH DNS LOOKUP LIST */
			dnslist[resolver1.GetFD()] = this;
			
			return true;
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
							if (std::string(usr->ip) == ip)
							{
								strlcpy(usr->host,hostname.c_str(),MAXBUF);
								strlcpy(usr->dhost,hostname.c_str(),MAXBUF);
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
					return true;
				if (resolver1.GetFD() != -1)
				{
					dnslist[resolver1.GetFD()] = NULL;
					hostname = resolver1.GetResult();
					if (usr)
					{
						if ((usr->registered > 3) || (hostname == ""))
						{
							usr->dns_done = true;
							return true;
						}
					}
					if (hostname != "")
					{
						resolver2.ForwardLookup(hostname);
						if (resolver2.GetFD())
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

bool lookup_dns(std::string nick)
{
	/* First attempt to find the nickname */
	userrec* u = Find(nick);
	if (u)
	{
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
	if ((fdcheck < 0) || (fdcheck > 65535))
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
	log(DEBUG,"DNS: Received an event for an invalid descriptor!");
}

