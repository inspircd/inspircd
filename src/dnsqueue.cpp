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

/* Now with added unF! ;) */

using namespace std;

#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include "inspircd_config.h"
#include <unistd.h>
#include <fcntl.h>
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
#include <errno.h>
#include <deque>
#include <errno.h>
#include <unistd.h>
#include <sched.h>
#include "connection.h"
#include "users.h"
#include "servers.h"
#include "ctables.h"
#include "globals.h"
#include "modules.h"
#include "dynamic.h"
#include "wildcard.h"
#include "message.h"
#include "mode.h"
#include "commands.h"
#include "xline.h"
#include "inspstring.h"
#include "dnsqueue.h"
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include "dns.h"

#ifdef GCC3
#define nspace __gnu_cxx
#else
#define nspace std
#endif

extern int MaxWhoResults;

extern std::vector<Module*> modules;
extern std::vector<std::string> module_names;
extern std::vector<ircd_module*> factory;
extern std::vector<int> fd_reap;

extern int MODCOUNT;

namespace nspace
{
#ifdef GCC34
        template<> struct hash<in_addr>
#else
        template<> struct nspace::hash<in_addr>
#endif
        {
                size_t operator()(const struct in_addr &a) const
                {
                        size_t q;
                        memcpy(&q,&a,sizeof(size_t));
                        return q;
                }
        };
#ifdef GCC34
        template<> struct hash<string>
#else
        template<> struct nspace::hash<string>
#endif
        {
                size_t operator()(const string &s) const
                {
                        char a[MAXBUF];
                        static struct hash<const char *> strhash;
                        strlcpy(a,s.c_str(),MAXBUF);
                        strlower(a);
                        return strhash(a);
                }
        };
}


struct StrHashComp
{

	bool operator()(const string& s1, const string& s2) const
	{
		char a[MAXBUF],b[MAXBUF];
		strlcpy(a,s1.c_str(),MAXBUF);
		strlcpy(b,s2.c_str(),MAXBUF);
		return (strcasecmp(a,b) == 0);
	}

};

struct InAddr_HashComp
{

	bool operator()(const in_addr &s1, const in_addr &s2) const
	{
		size_t q;
		size_t p;
		
		memcpy(&q,&s1,sizeof(size_t));
		memcpy(&p,&s2,sizeof(size_t));
		
		return (q == p);
	}

};


typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, StrHashComp> chan_hash;
typedef nspace::hash_map<in_addr,string*, nspace::hash<in_addr>, InAddr_HashComp> address_cache;
typedef std::deque<command_t> command_table;

extern user_hash clientlist;
extern chan_hash chanlist;
extern user_hash whowas;
extern command_table cmdlist;
extern address_cache IP;

extern ClassVector Classes;

extern char DNSServer[MAXBUF];

class Lookup {
private:
	DNS* resolver;
	char u[NICKMAX];
public:
	Lookup()
	{
		strcpy(u,"");
		resolver = NULL;
	}

	~Lookup()
	{
		if (resolver)
			delete resolver;
	}

	Lookup(std::string nick)
	{
		userrec* usr = Find(nick);
		if (usr)
		{
			log(DEBUG,"New Lookup class for %s with DNSServer set to '%s'",nick.c_str(),DNSServer);
			resolver = new DNS(std::string(DNSServer));
			resolver->ReverseLookup(std::string(usr->host));
			strlcpy(u,nick.c_str(),NICKMAX);
		}
	}

	bool Done()
	{
		userrec* usr = NULL;
		if (resolver->HasResult())
		{
			if (resolver->GetFD() != 0)
			{
				std::string hostname = resolver->GetResult();
				usr = Find(u);
				if (usr)
				{
					log(DEBUG,"Applying hostname lookup to %s: %s",usr->nick,hostname.c_str());
					if (hostname != "")
					{
						strlcpy(usr->host,hostname.c_str(),MAXBUF);
						WriteServ(usr->fd,"NOTICE Auth :Resolved your hostname: %s",hostname.c_str());
						usr->dns_done = true;
						return true;
					}
					return false;
				}
			}
			else
			{
				usr->dns_done = true;
				return true;
			}
		}
		return false;
	}

	int GetFD()
	{
		userrec* usr = Find(u);
		if (usr)
		{
			return usr->fd;
		}
		else return 0;
	}
};

typedef std::deque<Lookup> dns_queue;

dns_queue dnsq;

bool lookup_dns(std::string nick)
{
	userrec* u = Find(nick);
	if (u)
	{
		// place a new user into the queue...
		log(DEBUG,"Queueing DNS lookup for %s",u->nick);
		WriteServ(u->fd,"NOTICE Auth :Looking up your hostname...");
		Lookup L(nick);
		dnsq.push_back(L);
		return true;
	}
	return false;
}

void dns_poll()
{
	// do we have items in the queue?
	if (dnsq.size())
	{
		// are any ready, or stale?
		if (dnsq[0].Done() || (!dnsq[0].GetFD()))
		{
			if (dnsq[0].GetFD())
			{
				log(DEBUG,"****** DNS lookup for fd %d is complete. ******",dnsq[0].GetFD());
			}
			dnsq.pop_front();
		}
	}
}

