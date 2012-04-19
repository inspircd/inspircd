/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
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
CmdResult CommandDie::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (!ServerInstance->PassCompare(user, ServerInstance->Config->diepass, parameters[0].c_str(), ServerInstance->Config->powerhash))
	{
		{
			std::string diebuf = std::string("*** DIE command from ") + user->nick + "!" + user->ident + "@" + user->dhost + ". Terminating in " + ConvToStr(ServerInstance->Config->DieDelay) + " seconds.";
			ServerInstance->Logs->Log("COMMAND",SPARSE, diebuf);
			ServerInstance->SendError(diebuf);
		}

		if (ServerInstance->Config->DieDelay)
			sleep(ServerInstance->Config->DieDelay);

		ServerInstance->Exit(EXIT_STATUS_DIE);
	}
	else
	{
		ServerInstance->Logs->Log("COMMAND",SPARSE, "Failed /DIE command from %s!%s@%s", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "Failed DIE Command from %s!%s@%s.",user->nick.c_str(),user->ident.c_str(),user->host.c_str());
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}
