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
#include "commands/cmd_restart.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandRestart(Instance);
}

CmdResult CommandRestart::Handle (const std::vector<std::string>& parameters, User *user)
{
	ServerInstance->Logs->Log("COMMAND",DEFAULT,"Restart: %s",user->nick.c_str());
	if (!ServerInstance->PassCompare(user, ServerInstance->Config->restartpass, parameters[0].c_str(), ServerInstance->Config->powerhash))
	{
		ServerInstance->SNO->WriteGlobalSno('a', "RESTART command from %s!%s@%s, restarting server.", user->nick.c_str(), user->ident.c_str(), user->host.c_str());

		try
		{
			ServerInstance->Restart("Server restarting.");
		}
		catch (...)
		{
			/* We dont actually get here unless theres some fatal and unrecoverable error. */
			exit(0);
		}
	}
	else
	{
		ServerInstance->SNO->WriteGlobalSno('a', "Failed RESTART Command from %s!%s@%s.", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

