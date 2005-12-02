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

extern int MaxWhoResults;

extern std::vector<Module*> modules;
extern std::vector<std::string> module_names;
extern std::vector<ircd_module*> factory;

extern int MODCOUNT;

typedef nspace::hash_map<std::string, userrec*, nspace::hash<string>, irc::StrHashComp> user_hash;
typedef nspace::hash_map<std::string, chanrec*, nspace::hash<string>, irc::StrHashComp> chan_hash;
typedef nspace::hash_map<in_addr,string*, nspace::hash<in_addr>, irc::InAddr_HashComp> address_cache;
typedef nspace::hash_map<std::string, WhoWasUser*, nspace::hash<string>, irc::StrHashComp> whowas_hash;
typedef std::deque<command_t> command_table;

extern user_hash clientlist;
extern chan_hash chanlist;
extern whowas_hash whowas;
extern command_table cmdlist;

extern ClassVector Classes;

extern char DNSServer[MAXBUF];
long max_fd_alloc = 0;

extern time_t TIME;

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
			log(DEBUG,"New Lookup class for %s with DNSServer set to '%s'",nick.c_str(),DNSServer);
			resolver1.SetNS(std::string(DNSServer));
			if (!resolver1.ReverseLookup(std::string(usr->host)))
				return false;
			strlcpy(u,nick.c_str(),NICKMAX);
			return true;
		}
		return false;
	}

	bool Done()
	{
		if (hostname != "")
		{
			// doing forward lookup
			userrec* usr = NULL;
			if (resolver2.HasResult())
			{
				if (resolver2.GetFD() != 0)
				{
					std::string ip = resolver2.GetResultIP();
					log(DEBUG,"FORWARD RESULT! %s",ip.c_str());

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
								log(DEBUG,"Forward and reverse match, assigning hostname");
							}
							else
							{
								log(DEBUG,"AWOOGA! Forward lookup doesn't match reverse: R='%s',F='%s',IP='%s'",hostname.c_str(),ip.c_str(),usr->ip);
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
						usr->dns_done = true;
					return true;
				}
			}
		}
		else
		{
			// doing reverse lookup
			userrec* usr = NULL;
			if (resolver1.HasResult())
			{
				if (resolver1.GetFD() != 0)
				{
					hostname = resolver1.GetResult();
					log(DEBUG,"REVERSE RESULT! %s",hostname.c_str());
					usr = Find(u);
					if (usr)
					{
						if (usr->registered > 3)
						{
							usr->dns_done = true;
							return true;
						}
					}
					resolver2.ForwardLookup(hostname);
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

Lookup dnsq[255];

bool lookup_dns(std::string nick)
{
	userrec* u = Find(nick);
	if (u)
	{
		// place a new user into the queue...
		log(DEBUG,"Queueing DNS lookup for %s",u->nick);
		WriteServ(u->fd,"NOTICE Auth :Looking up your hostname...");
		Lookup L;
		if (L.DoLookup(nick))
		{
			for (int j = 0; j < 255; j++)
			{
				if (!dnsq[j].GetFD())
				{
					dnsq[j] = L;
					return true;
				}
			}
			// calculate the maximum value, this saves cpu time later
			for (int p = 0; p < 255; p++)
				if (dnsq[p].GetFD())
					max_fd_alloc = p;
		}
		else
		{
			return false;
		}
	}
	return false;
}

void dns_poll()
{
	// do we have items in the queue?
	for (int j = 0; j <= max_fd_alloc; j++)
	{
		// are any ready, or stale?
		if (dnsq[j].GetFD())
		{
			if (dnsq[j].Done())
			{
				dnsq[j].Reset();
			}
		}
	}
	// looks like someones freed an item, recalculate end of list.
	if ((!dnsq[max_fd_alloc].GetFD()) && (max_fd_alloc != 0))
		for (int p = 0; p < 255; p++)
			if (dnsq[p].GetFD())
				max_fd_alloc = p;

}
