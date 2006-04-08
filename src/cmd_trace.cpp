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
#include "commands.h"
#include "helperfuncs.h"
#include "cmd_trace.h"

extern user_hash clientlist;

void cmd_trace::Handle (char **parameters, int pcnt, userrec *user)
{
	for (user_hash::iterator i = clientlist.begin(); i != clientlist.end(); i++)
	{
		if (i->second)
		{
			if (isnick(i->second->nick))
			{
				if (*i->second->oper)
				{
					WriteServ(user->fd,"205 %s :Oper 0 %s",user->nick,i->second->nick);
				}
				else
				{
					WriteServ(user->fd,"204 %s :User 0 %s",user->nick,i->second->nick);
				}
			}
			else
			{
				WriteServ(user->fd,"203 %s :???? 0 [%s]",user->nick,i->second->host);
			}
		}
	}
}
