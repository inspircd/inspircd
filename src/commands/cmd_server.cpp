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

/** Handle /SERVER.
 */
class CommandServer : public Command
{
 public:
	/** Constructor for server.
	 */
	CommandServer ( Module* parent) : Command(parent,"SERVER") { works_before_reg = true;}
	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandServer::Handle (const std::vector<std::string>&, User *user)
{
	if (user->registered == REG_ALL)
	{
		user->WriteNumeric(ERR_ALREADYREGISTERED, ":You are already registered. (Perhaps your IRC client does not have a /SERVER command).");
	}
	else
	{
		user->WriteNumeric(ERR_NOTREGISTERED, ":You may not register as a server (servers have separate ports from clients, change your config)");
	}
	return CMD_FAILURE;
}

COMMAND_INIT(CommandServer)
