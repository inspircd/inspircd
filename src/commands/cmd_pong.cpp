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

/** Handle /PONG. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandPong : public Command
{
 public:
	/** Constructor for pong.
	 */
	CommandPong ( Module* parent) : Command(parent,"PONG", 0, 1) { Penalty = 0; syntax = "<ping-text>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandPong::Handle (const std::vector<std::string>&, User *user)
{
	// set the user as alive so they survive to next ping
	LocalUser* localuser = IS_LOCAL(user);
	if (localuser)
	{
		// Increase penalty unless we've sent a PING and this is the reply
		if (localuser->lastping)
			localuser->CommandFloodPenalty += 1000;
		else
			localuser->lastping = 1;
	}
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandPong)
