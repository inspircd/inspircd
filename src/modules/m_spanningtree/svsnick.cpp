/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

#include "commands.h"

CmdResult CommandSVSNick::Handle(const std::vector<std::string>& parameters, User *user)
{
	User* u = ServerInstance->FindNick(parameters[0]);

	if (u && IS_LOCAL(u))
	{
		std::string nick = parameters[1];
		if (isdigit(nick[0]))
			nick = u->uuid;

		if (!u->ForceNickChange(nick))
		{
			/* buh. UID them */
			if (!u->ForceNickChange(u->uuid))
			{
				ServerInstance->Users->QuitUser(u, "Nickname collision");
				return CMD_SUCCESS;
			}
		}

		u->age = ConvToInt(parameters[2]);
	}

	return CMD_SUCCESS;
}

RouteDescriptor CommandSVSNick::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	User* u = ServerInstance->FindNick(parameters[0]);
	if (u)
		return ROUTE_OPT_UCAST(u->server);
	return ROUTE_LOCALONLY;
}
