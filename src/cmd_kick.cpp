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

#include "inspircd_config.h"
#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_kick.h"

void cmd_kick::Handle (const char** parameters, int pcnt, userrec *user)
{
	char reason[MAXKICK];
	chanrec* c = FindChan(parameters[0]);
	userrec* u   = Find(parameters[1]);

	if (!u || !c)
	{
		WriteServ(user->fd, "401 %s %s :No such nick/channel", user->nick, u ? parameters[0] : parameters[1]);
		return;
	}

	if ((IS_LOCAL(user)) && (!c->HasUser(user)) && (!is_uline(user->server)))
	{
		WriteServ(user->fd, "442 %s %s :You're not on that channel!", user->nick, parameters[0]);
		return;
	}

	if (pcnt > 2)
	{
		strlcpy(reason, parameters[2], MAXKICK - 1);
	}
	else
	{
		strlcpy(reason, user->nick, MAXKICK - 1);
	}

	kick_channel(user, u, c, reason);
}
