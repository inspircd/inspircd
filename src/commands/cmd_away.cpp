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
#include "commands/cmd_away.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandAway(Instance);
}

/** Handle /AWAY
 */
CmdResult CommandAway::Handle (const std::vector<std::string>& parameters, User *user)
{
	int MOD_RESULT = 0;

	if ((parameters.size()) && (!parameters[0].empty()))
	{
		FOREACH_RESULT(I_OnSetAway, OnSetAway(user, parameters[0]));

		if (MOD_RESULT != 0 && IS_LOCAL(user))
			return CMD_FAILURE;

		user->awaytime = ServerInstance->Time();
		user->awaymsg.assign(parameters[0], 0, ServerInstance->Config->Limits.MaxAway);

		user->WriteNumeric(RPL_NOWAWAY, "%s :You have been marked as being away",user->nick.c_str());
	}
	else
	{
		FOREACH_RESULT(I_OnSetAway, OnSetAway(user, ""));

		if (MOD_RESULT != 0 && IS_LOCAL(user))
			return CMD_FAILURE;

		user->awaymsg.clear();
		user->WriteNumeric(RPL_UNAWAY, "%s :You are no longer marked as being away",user->nick.c_str());
	}

	return CMD_SUCCESS;
}
