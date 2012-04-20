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
#include "commands/cmd_unloadmodule.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandUnloadmodule(Instance);
}

CmdResult CommandUnloadmodule::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (ServerInstance->Modules->Unload(parameters[0].c_str()))
	{
		ServerInstance->SNO->WriteToSnoMask('a', "MODULE UNLOADED: %s unloaded %s", user->nick.c_str(), parameters[0].c_str());
		user->WriteNumeric(973, "%s %s :Module successfully unloaded.",user->nick.c_str(), parameters[0].c_str());
	}
	else
	{
		user->WriteNumeric(972, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}
