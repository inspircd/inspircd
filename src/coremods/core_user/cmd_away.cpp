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
#include "core_user.h"

CommandAway::CommandAway(Module* parent)
	: Command(parent, "AWAY", 0, 0)
{
	syntax = "[<message>]";
}

/** Handle /AWAY
 */
CmdResult CommandAway::Handle (const std::vector<std::string>& parameters, User *user)
{
	ModResult MOD_RESULT;

	if ((!parameters.empty()) && (!parameters[0].empty()))
	{
		FIRST_MOD_RESULT(OnSetAway, MOD_RESULT, (user, parameters[0]));

		if (MOD_RESULT == MOD_RES_DENY && IS_LOCAL(user))
			return CMD_FAILURE;

		user->awaytime = ServerInstance->Time();
		user->awaymsg.assign(parameters[0], 0, ServerInstance->Config->Limits.MaxAway);

		user->WriteNumeric(RPL_NOWAWAY, "You have been marked as being away");
	}
	else
	{
		FIRST_MOD_RESULT(OnSetAway, MOD_RESULT, (user, ""));

		if (MOD_RESULT == MOD_RES_DENY && IS_LOCAL(user))
			return CMD_FAILURE;

		user->awaymsg.clear();
		user->WriteNumeric(RPL_UNAWAY, "You are no longer marked as being away");
	}

	return CMD_SUCCESS;
}

RouteDescriptor CommandAway::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
