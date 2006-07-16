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
#include "commands/cmd_part.h"

extern InspIRCd* ServerInstance;

void cmd_part::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (ServerInstance->Parser->LoopCall(user, this, parameters, pcnt, 0))
		return;
	
	if (pcnt > 1)
	{
		del_channel(user,parameters[0],parameters[1],false);
	}
	else
	{
		del_channel(user,parameters[0],NULL,false);
	}
}
