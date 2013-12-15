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

#include "main.h"
#include "utils.h"
#include "commands.h"

CmdResult CommandSVSNick::Handle(const std::vector<std::string>& parameters, User *user)
{
	User* u = ServerInstance->FindNick(parameters[0]);

	if (u && IS_LOCAL(u))
	{
		std::string nick = parameters[1];
		if (isdigit(nick[0]))
			nick = u->uuid;

		// Don't update the TS if the nick is exactly the same
		if (u->nick == nick)
			return CMD_FAILURE;

		time_t NickTS = ConvToInt(parameters[2]);
		if (NickTS <= 0)
			return CMD_FAILURE;

		ModuleSpanningTree* st = (ModuleSpanningTree*)(Module*)creator;
		st->KeepNickTS = true;
		u->age = NickTS;

		if (!u->ForceNickChange(nick.c_str()))
		{
			/* buh. UID them */
			if (!u->ForceNickChange(u->uuid.c_str()))
			{
				ServerInstance->Users->QuitUser(u, "Nickname collision");
			}
		}

		st->KeepNickTS = false;
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
