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

/** Handle /NAMES. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandNames : public Command
{
 public:
	/** Constructor for names.
	 */
	CommandNames ( Module* parent) : Command(parent,"NAMES",0,0) { syntax = "{<channel>{,<channel>}}"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /NAMES
 */
CmdResult CommandNames::Handle (const std::vector<std::string>& parameters, User *user)
{
	Channel* c;

	if (!parameters.size())
	{
		user->WriteNumeric(366, "%s * :End of /NAMES list.",user->nick.c_str());
		return CMD_SUCCESS;
	}

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	c = ServerInstance->FindChan(parameters[0]);
	if (c)
	{
		c->UserList(user);
	}
	else
	{
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
	}

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandNames)
