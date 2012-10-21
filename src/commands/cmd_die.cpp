/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

/** Handle /DIE. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandDie : public Command
{
 public:
	/** Constructor for die.
	 */
	CommandDie ( Module* parent) : Command(parent,"DIE",1) { flags_needed = 'o'; syntax = "<password>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#include "exitcodes.h"

/** Handle /DIE
 */
CmdResult CommandDie::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (!ServerInstance->PassCompare(user, ServerInstance->Config->diepass, parameters[0].c_str(), ServerInstance->Config->powerhash))
	{
		{
			std::string diebuf = "*** DIE command from " + user->GetFullHost() + ". Terminating.";
			ServerInstance->Logs->Log("COMMAND",SPARSE, diebuf);
			ServerInstance->SendError(diebuf);
		}

		ServerInstance->Exit(EXIT_STATUS_DIE);
	}
	else
	{
		ServerInstance->Logs->Log("COMMAND",SPARSE, "Failed /DIE command from %s", user->GetFullRealHost().c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "Failed DIE Command from %s.", user->GetFullRealHost().c_str());
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandDie)
