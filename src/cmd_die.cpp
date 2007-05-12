/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "configreader.h"
#include "users.h"
#include "commands/cmd_die.h"
#include "exitcodes.h"

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
		ServerInstance->SendError("*** DIE command from %s!%s@%s, terminating...", user->nick, user->nick, user->ident, user->dhost);
		
		if (ServerInstance->Config->DieDelay)
			sleep(ServerInstance->Config->DieDelay);

		InspIRCd::Exit(EXIT_STATUS_DIE);
	}
	else
	{
		ServerInstance->Log(SPARSE, "Failed /DIE command from %s!%s@%s", user->nick, user->ident, user->host);
		ServerInstance->WriteOpers("*** Failed DIE Command from %s!%s@%s.",user->nick,user->ident,user->host);
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}
