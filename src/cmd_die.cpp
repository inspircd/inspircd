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

#include "inspircd.h"
#include "commands/cmd_die.h"
#include "exitcodes.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandDie(Instance);
}

/** Handle /DIE
 */
CmdResult CommandDie::Handle (const char** parameters, int pcnt, User *user)
{
	if (!strcmp(parameters[0],ServerInstance->Config->diepass))
	{
		std::string diebuf = std::string("*** DIE command from ") + user->nick + "!" + user->ident + "@" + user->dhost + ". Terminating in " + ConvToStr(ServerInstance->Config->DieDelay) + " seconds.";
		ServerInstance->Log(SPARSE, diebuf);
		ServerInstance->SendError(diebuf);
		
		if (ServerInstance->Config->DieDelay)
			sleep(ServerInstance->Config->DieDelay);

		ServerInstance->Exit(EXIT_STATUS_DIE);
	}
	else
	{
		ServerInstance->Log(SPARSE, "Failed /DIE command from %s!%s@%s", user->nick, user->ident, user->host);
		ServerInstance->WriteOpers("*** Failed DIE Command from %s!%s@%s.",user->nick,user->ident,user->host);
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}
