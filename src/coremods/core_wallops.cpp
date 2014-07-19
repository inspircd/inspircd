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

/** Handle /WALLOPS.
 */
class CommandWallops : public Command
{
	SimpleUserModeHandler wallopsmode;

 public:
	/** Constructor for wallops.
	 */
	CommandWallops(Module* parent)
		: Command(parent, "WALLOPS", 1, 1)
		, wallopsmode(parent, "wallops", 'w')
	{
		flags_needed = 'o';
		syntax = "<any-text>";
	}

	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

CmdResult CommandWallops::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string wallop("WALLOPS :");
	wallop.append(parameters[0]);

	const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
	for (UserManager::LocalList::const_iterator i = list.begin(); i != list.end(); ++i)
	{
		User* t = *i;
		if (t->IsModeSet(wallopsmode))
			t->WriteFrom(user, wallop);
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandWallops)
