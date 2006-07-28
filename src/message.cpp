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
#include "configreader.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/utsname.h>
#include <time.h>
#include <string>
#include <ext/hash_map>
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
#include "commands.h"
#include "message.h"
#include "inspstring.h"
#include "dns.h"
#include "helperfuncs.h"

extern int MODCOUNT;
extern std::vector<Module*> modules;
extern std::vector<ircd_module*> factory;
extern time_t TIME;
extern ServerConfig* Config;

/* return 0 or 1 depending if users u and u2 share one or more common channels
 * (used by QUIT, NICK etc which arent channel specific notices)
 *
 * The old algorithm in 1.0 for this was relatively inefficient, iterating over
 * the first users channels then the second users channels within the outer loop,
 * therefore it was a maximum of x*y iterations (upon returning 0 and checking
 * all possible iterations). However this new function instead checks against the
 * channel's userlist in the inner loop which is a std::map<userrec*,userrec*>
 * and saves us time as we already know what pointer value we are after.
 * Don't quote me on the maths as i am not a mathematician or computer scientist,
 * but i believe this algorithm is now x+(log y) maximum iterations instead.
 */
int common_channels(userrec *u, userrec *u2)
{
	if ((!u) || (!u2) || (u->registered != REG_ALL) || (u2->registered != REG_ALL))
		return 0;

	/* Outer loop */
	for (std::vector<ucrec*>::const_iterator i = u->chans.begin(); i != u->chans.end(); i++)
	{
		/* Fetch the channel from the user */
		ucrec* user_channel = *i;

		if (user_channel->channel)
		{
			/* Eliminate the inner loop (which used to be ~equal in size to the outer loop)
			 * by replacing it with a map::find which *should* be more efficient
			 */
			if (user_channel->channel->HasUser(u2))
				return 1;
		}
	}
	return 0;
}

void Blocking(int s)
{
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags ^ O_NONBLOCK);
}

void NonBlocking(int s)
{
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
}

int CleanAndResolve (char *resolvedHost, const char *unresolvedHost, bool forward, unsigned long timeout)
{
	bool ok;
	std::string ipaddr;

	DNS d(Config->DNSServer);
	if (forward)
		ok = d.ForwardLookup(unresolvedHost, false);
	else
		ok = d.ReverseLookup(unresolvedHost, false);
	if (!ok)
		return 0;
	time_t T = time(NULL)+timeout;
	while ((!d.HasResult()) && (time(NULL)<T));
	if (forward)
		ipaddr = d.GetResultIP();
	else
		ipaddr = d.GetResult();
	strlcpy(resolvedHost,ipaddr.c_str(),MAXBUF);
	return (ipaddr != "");
}

int c_count(userrec* u)
{
	int z = 0;
	for (std::vector<ucrec*>::const_iterator i = u->chans.begin(); i != u->chans.end(); i++)
		if ((*i)->channel)
			z++;
	return z;

}

void ChangeName(userrec* user, const char* gecos)
{
	if (user->fd > -1)
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnChangeLocalUserGECOS,OnChangeLocalUserGECOS(user,gecos));
		if (MOD_RESULT)
			return;
		FOREACH_MOD(I_OnChangeName,OnChangeName(user,gecos));
	}
	strlcpy(user->fullname,gecos,MAXGECOS+1);
}

void ChangeDisplayedHost(userrec* user, const char* host)
{
	if (user->fd > -1)
	{
		int MOD_RESULT = 0;
		FOREACH_RESULT(I_OnChangeLocalUserHost,OnChangeLocalUserHost(user,host));
		if (MOD_RESULT)
			return;
		FOREACH_MOD(I_OnChangeHost,OnChangeHost(user,host));
	}
	strlcpy(user->dhost,host,63);
	WriteServ(user->fd,"396 %s %s :is now your hidden host",user->nick,user->dhost);
}

/* verify that a user's ident and nickname is valid */

int isident(const char* n)
{
	if (!n || !*n)
	{
		return 0;
	}
	for (char* i = (char*)n; *i; i++)
	{
		if ((*i >= 'A') && (*i <= '}'))
		{
			continue;
		}
		if (((*i >= '0') && (*i <= '9')) || (*i == '-') || (*i == '.'))
		{
			continue;
		}
		return 0;
	}
	return 1;
}


int isnick(const char* n)
{
	if (!n || !*n)
	{
		return 0;
	}
	int p = 0;
	for (char* i = (char*)n; *i; i++, p++)
	{
		/* "A"-"}" can occur anywhere in a nickname */
		if ((*i >= 'A') && (*i <= '}'))
		{
			continue;
		}
		/* "0"-"9", "-" can occur anywhere BUT the first char of a nickname */
		if ((((*i >= '0') && (*i <= '9')) || (*i == '-')) && (i > n))
		{
			continue;
		}
		/* invalid character! abort */
		return 0;
	}
	return (p < NICKMAX - 1);
}

/* returns the status character for a given user on a channel, e.g. @ for op,
 * % for halfop etc. If the user has several modes set, the highest mode
 * the user has must be returned. */

const char* cmode(userrec *user, chanrec *chan)
{
	if ((!user) || (!chan))
	{
		log(DEFAULT,"*** BUG *** cmode was given an invalid parameter");
		return "";
	}

	for (std::vector<ucrec*>::const_iterator i = user->chans.begin(); i != user->chans.end(); i++)
	{
		if ((*i)->channel == chan)
		{
			if (((*i)->uc_modes & UCMODE_OP) > 0)
			{
				return "@";
			}
			if (((*i)->uc_modes & UCMODE_HOP) > 0)
			{
				return "%";
			}
			if (((*i)->uc_modes & UCMODE_VOICE) > 0)
			{
				return "+";
			}
			return "";
		}
	}
	return "";
}

int cflags(userrec *user, chanrec *chan)
{
	if ((!chan) || (!user))
		return 0;

	for (std::vector<ucrec*>::const_iterator i = user->chans.begin(); i != user->chans.end(); i++)
	{
		if ((*i)->channel == chan)
		{
			return (*i)->uc_modes;
		}
	}
	return 0;
}

/* returns the status value for a given user on a channel, e.g. STATUS_OP for
 * op, STATUS_VOICE for voice etc. If the user has several modes set, the
 * highest mode the user has must be returned. */

int cstatus(userrec *user, chanrec *chan)
{
	if ((!chan) || (!user))
	{
		log(DEFAULT,"*** BUG *** cstatus was given an invalid parameter");
		return 0;
	}

	if (is_uline(user->server))
		return STATUS_OP;

	for (std::vector<ucrec*>::const_iterator i = user->chans.begin(); i != user->chans.end(); i++)
	{
		if ((*i)->channel == chan)
		{
			if (((*i)->uc_modes & UCMODE_OP) > 0)
			{
				return STATUS_OP;
			}
			if (((*i)->uc_modes & UCMODE_HOP) > 0)
			{
				return STATUS_HOP;
			}
			if (((*i)->uc_modes & UCMODE_VOICE) > 0)
			{
				return STATUS_VOICE;
			}
			return STATUS_NORMAL;
		}
	}
	return STATUS_NORMAL;
}

std::string chlist(userrec *user,userrec* source)
{
	std::string list;
	
	if (!user || !source)
		return "";
	
	for (std::vector<ucrec*>::const_iterator i = user->chans.begin(); i != user->chans.end(); i++)
	{
		ucrec* rec = *i;
		
		if(rec->channel && rec->channel->name)
		{	
			/* If the target is the same as the sender, let them see all their channels.
			 * If the channel is NOT private/secret OR the user shares a common channel
			 * If the user is an oper, and the <options:operspywhois> option is set.
			 */
			if ((source == user) || (*source->oper && Config->OperSpyWhois) || (((!rec->channel->modes[CM_PRIVATE]) && (!rec->channel->modes[CM_SECRET])) || (rec->channel->HasUser(source))))
			{
				list.append(cmode(user, rec->channel)).append(rec->channel->name).append(" ");
			}
		}
	}
	return list;
}
