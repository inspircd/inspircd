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
	if (pcnt > 1)
	{
		if (ServerInstance->Parser->LoopCall(this,parameters,pcnt,user,0,pcnt-2,0))
			return;
		del_channel(user,parameters[0],parameters[1],false);
	}
	else
	{
		if (ServerInstance->Parser->LoopCall(this,parameters,pcnt,user,0,pcnt-1,0))
			return;
		del_channel(user,parameters[0],NULL,false);
	}
}
