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
#include "core_channel.h"

CommandJoin::CommandJoin(Module* parent)
	: SplitCommand(parent, "JOIN", 1, 2)
{
	syntax = "<channel>[,<channel>]+ [<key>[,<key>]+]";
	Penalty = 2;
}

/** Handle /JOIN
 */
CmdResult CommandJoin::HandleLocal(LocalUser* user, const Params& parameters)
{
	if (parameters.size() > 1)
	{
		if (CommandParser::LoopCall(user, this, parameters, 0, 1, false))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0]))
		{
			Channel::JoinUser(user, parameters[0], false, parameters[1]);
			return CMD_SUCCESS;
		}
	}
	else
	{
		if (CommandParser::LoopCall(user, this, parameters, 0, -1, false))
			return CMD_SUCCESS;

		if (ServerInstance->IsChannel(parameters[0]))
		{
			Channel::JoinUser(user, parameters[0]);
			return CMD_SUCCESS;
		}
	}

	user->WriteNumeric(ERR_BADCHANMASK, parameters[0], "Invalid channel name");
	return CMD_FAILURE;
}
