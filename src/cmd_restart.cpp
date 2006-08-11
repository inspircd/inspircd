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

#include "configreader.h"
#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_restart.h"



void cmd_restart::Handle (const char** parameters, int pcnt, userrec *user)
{
	char *argv[32];
	log(DEFAULT,"Restart: %s",user->nick);
	if (!strcmp(parameters[0],ServerInstance->Config->restartpass))
	{
		ServerInstance->WriteOpers("*** RESTART command from %s!%s@%s, restarting server.",user->nick,user->ident,user->host);

		argv[0] = ServerInstance->Config->MyExecutable;
		argv[1] = "-wait";
		if (ServerInstance->Config->nofork)
		{
			argv[2] = "-nofork";
		}
		else
		{
			argv[2] = NULL;
		}
		argv[3] = NULL;
		
		// close ALL file descriptors
		ServerInstance->SendError("Server restarting.");
		sleep(1);
		for (int i = 0; i < MAX_DESCRIPTORS; i++)
		{
			shutdown(i,2);
    			close(i);
		}
		sleep(2);
		
		execv(ServerInstance->Config->MyExecutable,argv);

		exit(0);
	}
	else
	{
		ServerInstance->WriteOpers("*** Failed RESTART Command from %s!%s@%s.",user->nick,user->ident,user->host);
	}
}
