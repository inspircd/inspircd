/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "commands/cmd_names.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandNames(Instance);
}

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
		if ((c->IsModeSet('s')) && (!c->HasUser(user)))
		{
		      user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), c->name.c_str());
		      return CMD_FAILURE;
		}
		c->UserList(user);
	}
	else
	{
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
	}

	return CMD_SUCCESS;
}
