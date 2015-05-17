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
#include "exitcodes.h"
#include "core_oper.h"

CommandDie::CommandDie(Module* parent)
	: Command(parent, "DIE", 1)
{
	flags_needed = 'o';
	syntax = "<password>";
}

static void QuitAll()
{
	const std::string quitmsg = "Server shutdown";
	const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
	while (!list.empty())
		ServerInstance->Users.QuitUser(list.front(), quitmsg);
}

/** Handle /DIE
 */
CmdResult CommandDie::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (DieRestart::CheckPass(user, parameters[0], "diepass"))
	{
		{
			std::string diebuf = "*** DIE command from " + user->GetFullHost() + ". Terminating.";
			ServerInstance->Logs->Log("COMMAND", LOG_SPARSE, diebuf);
			ServerInstance->SendError(diebuf);
		}

		QuitAll();
		ServerInstance->Exit(EXIT_STATUS_DIE);
	}
	else
	{
		ServerInstance->Logs->Log("COMMAND", LOG_SPARSE, "Failed /DIE command from %s", user->GetFullRealHost().c_str());
		ServerInstance->SNO->WriteGlobalSno('a', "Failed DIE Command from %s.", user->GetFullRealHost().c_str());
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}
