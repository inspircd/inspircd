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
#include "commands/cmd_quit.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandQuit(Instance);
}

CmdResult CommandQuit::Handle (const std::vector<std::string>& parameters, User *user)
{

	std::string quitmsg;

	if (IS_LOCAL(user))
	{
		if (*ServerInstance->Config->FixedQuit)
			quitmsg = ServerInstance->Config->FixedQuit;
		else
			quitmsg = parameters.size() ?
				ServerInstance->Config->PrefixQuit + std::string(parameters[0]) + ServerInstance->Config->SuffixQuit
				: "Client exited";
	}
	else
		quitmsg = parameters.size() ? parameters[0] : "Client exited";

	ServerInstance->Users->QuitUser(user, quitmsg);

	return CMD_SUCCESS;
}

