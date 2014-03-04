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

/** Handle /PONG.
 */
class CommandPong : public Command
{
 public:
	/** Constructor for pong.
	 */
	CommandPong ( Module* parent) : Command(parent,"PONG", 0, 1) { Penalty = 0; syntax = "<ping-text>"; }
	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandPong::Handle (const std::vector<std::string>&, User *user)
{
	// set the user as alive so they survive to next ping
	if (IS_LOCAL(user))
		IS_LOCAL(user)->lastping = 1;
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandPong)
