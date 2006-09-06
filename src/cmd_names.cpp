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
#include "commands/cmd_names.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_names(Instance);
}

CmdResult cmd_names::Handle (const char** parameters, int pcnt, userrec *user)
{
	chanrec* c;

	if (!pcnt)
	{
		user->WriteServ("366 %s * :End of /NAMES list.",user->nick);
		return CMD_SUCCESS;
	}

	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return CMD_SUCCESS;

	c = ServerInstance->FindChan(parameters[0]);
	if (c)
	{
		if ((c->modes[CM_SECRET]) && (!c->HasUser(user)))
		{
		      user->WriteServ("401 %s %s :No such nick/channel",user->nick, c->name);
		      return CMD_FAILURE;
		}
		c->UserList(user);
		user->WriteServ("366 %s %s :End of /NAMES list.", user->nick, c->name);
	}
	else
	{
		user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
	}

	return CMD_SUCCESS;
}
