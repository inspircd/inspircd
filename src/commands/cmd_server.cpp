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
#include "commands/cmd_server.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandServer(Instance);
}

CmdResult CommandServer::Handle (const std::vector<std::string>&, User *user)
{
	if (user->registered == REG_ALL)
	{
		user->WriteNumeric(ERR_ALREADYREGISTERED, "%s :You are already registered. (Perhaps your IRC client does not have a /SERVER command).",user->nick.c_str());
	}
	else
	{
		user->WriteNumeric(ERR_NOTREGISTERED, "%s :You may not register as a server (servers have seperate ports from clients, change your config)",command.c_str());
	}
	return CMD_FAILURE;
}
