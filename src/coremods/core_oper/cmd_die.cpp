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
	: Command(parent, "DIE", 1, 1)
{
	flags_needed = 'o';
	syntax = "<servername>";
}

void DieRestart::SendError(const std::string& message)
{
	ClientProtocol::Messages::Error errormsg(message);
	ClientProtocol::Event errorevent(ServerInstance->GetRFCEvents().error, errormsg);
	const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
	for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		LocalUser* user = *i;
		if (user->registered == REG_ALL)
		{
			user->WriteNotice(message);
		}
		else
		{
			// Unregistered connections receive ERROR, not a NOTICE
			user->Send(errorevent);
		}
	}
}

/** Handle /DIE
 */
CmdResult CommandDie::Handle(User* user, const Params& parameters)
{
	if (irc::equals(parameters[0], ServerInstance->Config->ServerName))
	{
		{
			std::string diebuf = "*** DIE command from " + user->GetFullHost() + ". Terminating.";
			ServerInstance->Logs.Log(MODNAME, LOG_SPARSE, diebuf);
			DieRestart::SendError(diebuf);
		}

		ServerInstance->Exit(EXIT_STATUS_DIE);
	}
	else
	{
		ServerInstance->Logs.Log(MODNAME, LOG_SPARSE, "Failed /DIE command from %s", user->GetFullRealHost().c_str());
		ServerInstance->SNO.WriteGlobalSno('a', "Failed DIE command from %s.", user->GetFullRealHost().c_str());
		return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}
