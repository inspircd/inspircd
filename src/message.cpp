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
 * (used by QUIT, NICK etc which arent channel specific notices) */

int common_channels(userrec *u, userrec *u2)
{
	if ((!u) || (!u2) || (u->registered != 7) || (u2->registered != 7))
	{
		return 0;
	}
	for (std::vector<ucrec*>::const_iterator i = u->chans.begin(); i != u->chans.end(); i++)
	{
		for (std::vector<ucrec*>::const_iterator z = u2->chans.begin(); z != u2->chans.end(); z++)
		{
			if ((((ucrec*)(*i))->channel != NULL) && (((ucrec*)(*z))->channel != NULL))
			{
				if ((((ucrec*)(*i))->channel == ((ucrec*)(*z))->channel))
				{
					if ((c_count(u)) && (c_count(u2)))
					{
						return 1;
					}
				}
			}
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

int CleanAndResolve (char *resolvedHost, const char *unresolvedHost, bool forward)
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
	time_t T = time(NULL)+1;
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
		if (((ucrec*)(*i))->channel)
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
		if (strchr(".-0123456789",*i))
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
		/* can occur anywhere in a nickname */
		if ((*i >= 'A') && (*i <= '}'))
		{
			continue;
		}
		/* can occur anywhere BUT the first char of a nickname */
		if ((strchr("-0123456789",*i)) && (i > n))
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
		if (((ucrec*)(*i))->channel == chan)
		{
			if ((((ucrec*)(*i))->uc_modes & UCMODE_OP) > 0)
			{
				return "@";
			}
			if ((((ucrec*)(*i))->uc_modes & UCMODE_HOP) > 0)
			{
				return "%";
			}
			if ((((ucrec*)(*i))->uc_modes & UCMODE_VOICE) > 0)
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
		if (((ucrec*)(*i))->channel == chan)
		{
			return ((ucrec*)(*i))->uc_modes;
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
		if (((ucrec*)(*i))->channel == chan)
		{
			if ((((ucrec*)(*i))->uc_modes & UCMODE_OP) > 0)
			{
				return STATUS_OP;
			}
			if ((((ucrec*)(*i))->uc_modes & UCMODE_HOP) > 0)
			{
				return STATUS_HOP;
			}
			if ((((ucrec*)(*i))->uc_modes & UCMODE_VOICE) > 0)
			{
				return STATUS_VOICE;
			}
			return STATUS_NORMAL;
		}
	}
	return STATUS_NORMAL;
}

void TidyBan(char *ban)
{
	if (!ban) {
		log(DEFAULT,"*** BUG *** TidyBan was given an invalid parameter");
		return;
	}
	
	char temp[MAXBUF],NICK[MAXBUF],IDENT[MAXBUF],HOST[MAXBUF];

	strlcpy(temp,ban,MAXBUF);

	char* pos_of_pling = strchr(temp,'!');
	char* pos_of_at = strchr(temp,'@');

	pos_of_pling[0] = '\0';
	pos_of_at[0] = '\0';
	pos_of_pling++;
	pos_of_at++;

	strlcpy(NICK,temp,NICKMAX-1);
	strlcpy(IDENT,pos_of_pling,IDENTMAX+1);
	strlcpy(HOST,pos_of_at,63);

	snprintf(ban,MAXBUF,"%s!%s@%s",NICK,IDENT,HOST);
}

char lst[MAXBUF];

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
			/* XXX - Why does this check need to be here at all? :< */
			/* Commenting this out until someone finds a case where we need it */
			//if (lst.find(rec->channel->name) == std::string::npos)
			//{
			
				/*
				 * If the target is the same as the sender, let them see all their channels.
				 * If the channel is NOT private/secret AND the user is not invisible.
				 * If the user is an oper, and the <options:operspywhois> option is set.
				 */
				if ((source == user) || (*source->oper && Config->OperSpyWhois) || (((!rec->channel->modes[CM_PRIVATE]) && (!rec->channel->modes[CM_SECRET]) && !(user->modes[UM_INVISIBLE])) || (rec->channel->HasUser(source))))
				{
					list.append(cmode(user, rec->channel)).append(rec->channel->name).append(" ");
				}
			//}
		}
	}
	
	return list;
}
