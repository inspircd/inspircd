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
#include "commands/cmd_kick.h"

extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_kick(Instance);
}

/** Handle /KICK
 */
CmdResult cmd_kick::Handle (const char** parameters, int pcnt, userrec *user)
{
	char reason[MAXKICK];
	chanrec* c = ServerInstance->FindChan(parameters[0]);
	userrec* u = ServerInstance->FindNick(parameters[1]);

	if (!u || !c)
	{
		user->WriteServ( "401 %s %s :No such nick/channel", user->nick, u ? parameters[0] : parameters[1]);
		return CMD_FAILURE;
	}

	if ((IS_LOCAL(user)) && (!c->HasUser(user)) && (!ServerInstance->ULine(user->server)))
	{
		user->WriteServ( "442 %s %s :You're not on that channel!", user->nick, parameters[0]);
		return CMD_FAILURE;
	}

	if (pcnt > 2)
	{
		strlcpy(reason, parameters[2], MAXKICK - 1);
	}
	else
	{
		strlcpy(reason, user->nick, MAXKICK - 1);
	}

	if (!c->KickUser(user, u, reason))
		/* Nobody left here, delete the chanrec */
		delete c;

	return CMD_SUCCESS;
}
