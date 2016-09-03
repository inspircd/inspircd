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
#include "commands.h"

CmdResult CommandSVSNick::Handle(User* user, std::vector<std::string>& parameters)
{
	User* u = ServerInstance->FindNick(parameters[0]);

	if (u && IS_LOCAL(u))
	{
		// The 4th parameter is optional and it is the expected nick TS of the target user. If this parameter is
		// present and it doesn't match the user's nick TS, the SVSNICK is not acted upon.
		// This makes it possible to detect the case when services wants to change the nick of a user, but the
		// user changes their nick before the SVSNICK arrives, making the SVSNICK nick change (usually to a guest nick)
		// unnecessary. Consider the following for example:
		//
		// 1. test changes nick to Attila which is protected by services
		// 2. Services SVSNICKs the user to Guest12345
		// 3. Attila changes nick to Attila_ which isn't protected by services
		// 4. SVSNICK arrives
		// 5. Attila_ gets his nick changed to Guest12345 unnecessarily
		//
		// In this case when the SVSNICK is processed the target has already changed his nick to something
		// which isn't protected, so changing the nick again to a Guest nick is not desired.
		// However, if the expected nick TS parameter is present in the SVSNICK then the nick change in step 5
		// won't happen because the timestamps won't match.
		if (parameters.size() > 3)
		{
			time_t ExpectedTS = ConvToInt(parameters[3]);
			if (u->age != ExpectedTS)
				return CMD_FAILURE; // Ignore SVSNICK
		}

		std::string nick = parameters[1];
		if (isdigit(nick[0]))
			nick = u->uuid;

		time_t NickTS = ConvToInt(parameters[2]);
		if (NickTS <= 0)
			return CMD_FAILURE;

		if (!u->ChangeNick(nick, NickTS))
		{
			// Changing to 'nick' failed (it may already be in use), change to the uuid
			u->ChangeNick(u->uuid);
		}
	}

	return CMD_SUCCESS;
}

RouteDescriptor CommandSVSNick::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return ROUTE_OPT_UCAST(parameters[0]);
}
