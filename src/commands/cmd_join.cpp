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

/** Handle /JOIN. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandJoin : public Command
{
 public:
	/** Constructor for join.
	 */
	CommandJoin ( Module* parent) : Command(parent,"JOIN", 1, 2) { syntax = "<channel>{,<channel>} {<key>{,<key>}}"; Penalty = 2; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /JOIN
 */
CmdResult CommandJoin::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 1)
	{
		if (ServerInstance->Parser->LoopCall(user, this, parameters, 0, 1, false))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0].c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			Channel::JoinUser(user, parameters[0].c_str(), false, parameters[1].c_str(), false);
			return CMD_SUCCESS;
		}
	}
	else
	{
		if (ServerInstance->Parser->LoopCall(user, this, parameters, 0, -1, false))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0].c_str(), ServerInstance->Config->Limits.ChanMax))
		{
			Channel::JoinUser(user, parameters[0].c_str(), false, "", false);
			return CMD_SUCCESS;
		}
	}

	user->WriteNumeric(ERR_NOSUCHCHANNEL, "%s %s :Invalid channel name",user->nick.c_str(), parameters[0].c_str());
	return CMD_FAILURE;
}

COMMAND_INIT(CommandJoin)
