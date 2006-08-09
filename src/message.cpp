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
