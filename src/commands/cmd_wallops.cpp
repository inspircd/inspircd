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

/** Handle /WALLOPS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWallops : public Command
{
 public:
	/** Constructor for wallops.
	 */
	CommandWallops ( Module* parent) : Command(parent,"WALLOPS",1,1) { flags_needed = 'o'; syntax = "<any-text>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandWallops::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string wallop("WALLOPS :");
	wallop.append(parameters[0]);

	for (LocalUserList::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); i++)
	{
		User* t = *i;
		if (t->IsModeSet('w'))
			user->WriteTo(t,wallop);
	}

	FOREACH_MOD(I_OnWallops,OnWallops(user,parameters[0]));
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandWallops)
