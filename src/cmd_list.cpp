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
#include "inspircd.h"
#include "commands/cmd_list.h"
#include "wildcard.h"

void cmd_list::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("321 %s Channel :Users Name",user->nick);
	for (chan_hash::const_iterator i = ServerInstance->chanlist.begin(); i != ServerInstance->chanlist.end(); i++)
	{
		// attempt to match a glob pattern
		if (pcnt && !match(i->second->name, parameters[0]))
			continue;
		// if the channel is not private/secret, OR the user is on the channel anyway
		bool n = i->second->HasUser(user);
		if (((!(i->second->modes[CM_PRIVATE])) && (!(i->second->modes[CM_SECRET]))) || (n))
		{
			long users = i->second->GetUserCounter();
			if (users)
				user->WriteServ("322 %s %s %d :[+%s] %s",user->nick,i->second->name,users,i->second->ChanModes(n),i->second->topic);
		}
	}
	user->WriteServ("323 %s :End of channel list.",user->nick);
}
