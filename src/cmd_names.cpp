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

#include "inspircd.h"
#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_names.h"

extern InspIRCd* ServerInstance;

void cmd_names::Handle (const char** parameters, int pcnt, userrec *user)
{
	chanrec* c;

	if (!pcnt)
	{
		WriteServ(user->fd,"366 %s * :End of /NAMES list.",user->nick);
		return;
	}

	if (ServerInstance->Parser->LoopCall(this,parameters,pcnt,user,0,pcnt-1,0))
		return;
	c = FindChan(parameters[0]);
	if (c)
	{
		if ((c->modes[CM_SECRET]) && (!c->HasUser(user)))
		{
		      WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, c->name);
		      return;
		}
		userlist(user,c);
		WriteServ(user->fd,"366 %s %s :End of /NAMES list.", user->nick, c->name);
	}
	else
	{
		WriteServ(user->fd,"401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}
}
