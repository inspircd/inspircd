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
#include "commands/cmd_die.h"

extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_die(Instance);
}

/** Handle /DIE
 */
CmdResult cmd_die::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (!strcmp(parameters[0],ServerInstance->Config->diepass))
	{
		ServerInstance->Log(SPARSE, "/DIE command from %s!%s@%s, terminating in %d seconds...", user->nick, user->ident, user->host, ServerInstance->Config->DieDelay);
		
		/* This would just be WriteOpers(), but as we just sleep() and then die then the write buffers never get flushed.
		 * so we iterate the oper list, writing the message and immediately trying to flush their write buffer.
		 */
		
		for (std::vector<userrec*>::iterator i = ServerInstance->all_opers.begin(); i != ServerInstance->all_opers.end(); i++)
		{
			userrec* a = *i;
			
			if (IS_LOCAL(a) && (a->modes[UM_SERVERNOTICE]))
			{
				a->WriteServ("NOTICE %s :*** DIE command from %s!%s@%s, terminating...", a->nick, a->nick, a->ident, a->host);
				a->FlushWriteBuf();
			}
		}
		
		sleep(ServerInstance->Config->DieDelay);
		InspIRCd::Exit(ERROR);
	}
	else
	{
		ServerInstance->Log(SPARSE, "Failed /DIE command from %s!%s@%s", user->nick, user->ident, user->host);
		ServerInstance->WriteOpers("*** Failed DIE Command from %s!%s@%s.",user->nick,user->ident,user->host);
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}
