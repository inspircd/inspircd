/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "users.h"
#include "ctables.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_list.h"

extern chan_hash chanlist;

void cmd_list::Handle (const char** parameters, int pcnt, userrec *user)
{
	WriteServ(user->fd,"321 %s Channel :Users Name",user->nick);
	for (chan_hash::const_iterator i = chanlist.begin(); i != chanlist.end(); i++)
	{
		// if the channel is not private/secret, OR the user is on the channel anyway
		bool n = i->second->HasUser(user);
		if (((!(i->second->modes[CM_PRIVATE])) && (!(i->second->modes[CM_SECRET]))) || (n))
		{
			long users = usercount(i->second);
			if (users)
				WriteServ(user->fd,"322 %s %s %d :[+%s] %s",user->nick,i->second->name,users,chanmodes(i->second,n),i->second->topic);
		}
	}
	WriteServ(user->fd,"323 %s :End of channel list.",user->nick);
}
