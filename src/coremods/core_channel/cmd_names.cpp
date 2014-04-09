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

CommandNames::CommandNames(Module* parent)
	: Command(parent, "NAMES", 0, 0)
	, secretmode(parent, "secret")
{
	syntax = "{<channel>{,<channel>}}";
}

/** Handle /NAMES
 */
CmdResult CommandNames::Handle (const std::vector<std::string>& parameters, User *user)
{
	Channel* c;

	if (!parameters.size())
	{
		user->WriteNumeric(RPL_ENDOFNAMES, "* :End of /NAMES list.");
		return CMD_SUCCESS;
	}

	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CMD_SUCCESS;

	c = ServerInstance->FindChan(parameters[0]);
	if (c)
	{
		// Show the NAMES list if one of the following is true:
		// - the channel is not secret
		// - the user doing the /NAMES is inside the channel
		// - the user doing the /NAMES has the channels/auspex privilege

		bool has_user = c->HasUser(user);
		if ((!c->IsModeSet(secretmode)) || (has_user) || (user->HasPrivPermission("channels/auspex")))
		{
			c->UserList(user);
			return CMD_SUCCESS;
		}
	}

	user->WriteNumeric(ERR_NOSUCHNICK, "%s :No such nick/channel", parameters[0].c_str());
	return CMD_FAILURE;
}
