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
#include "commands/cmd_join.h"

extern InspIRCd* ServerInstance;

void cmd_join::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (ServerInstance->Parser->LoopCall(this, parameters, pcnt, user, 0, 0, 1))
		return;

	if (IsValidChannelName(parameters[0]))
	{
		add_channel(user, parameters[0], parameters[1], false);
	}
	else
	{
		WriteServ(user->fd,"403 %s %s :Invalid channel name",user->nick, parameters[0]);
	}
}
