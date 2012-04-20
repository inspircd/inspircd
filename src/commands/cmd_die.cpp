/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
