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

class CommandMap : public Command
{
 public:
	/** Constructor for map.
	 */
	CommandMap ( Module* parent) : Command(parent,"MAP",0,0) { Penalty=2; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /MAP
 */
CmdResult CommandMap::Handle (const std::vector<std::string>&, User *user)
{
	// as with /LUSERS this does nothing without a linking
	// module to override its behaviour and display something
	// better.

	if (IS_OPER(user))
	{
		user->WriteNumeric(006, "%s :%s [%s]", user->nick.c_str(), ServerInstance->Config->ServerName.c_str(), ServerInstance->Config->GetSID().c_str());
		user->WriteNumeric(007, "%s :End of /MAP", user->nick.c_str());
		return CMD_SUCCESS;
	}
	user->WriteNumeric(006, "%s :%s",user->nick.c_str(),ServerInstance->Config->ServerName.c_str());
	user->WriteNumeric(007, "%s :End of /MAP",user->nick.c_str());

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandMap)
