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

/** Handle /SQUIT.
 */
class CommandSquit : public Command
{
 public:
	/** Constructor for squit.
	 */
	CommandSquit ( Module* parent) : Command(parent,"SQUIT",1,2) { flags_needed = 'o'; syntax = "<servername> [<reason>]"; }
	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};


/*
 * This is handled by the server linking module, if necessary. Do not remove this stub.
 */


CmdResult CommandSquit::Handle (const std::vector<std::string>&, User *user)
{
	user->WriteServ( "NOTICE %s :Look into loading a linking module (like m_spanningtree) if you want this to do anything useful.", user->nick.c_str());
	return CMD_FAILURE;
}

COMMAND_INIT(CommandSquit)
