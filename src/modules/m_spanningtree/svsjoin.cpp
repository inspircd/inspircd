/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008-2009 Robin Burchell <robin+git@viroteck.net>
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

CmdResult CommandSVSJoin::Handle(User* user, Params& parameters)
{
	// Check for valid channel name
	if (!ServerInstance->Channels.IsChannel(parameters[1]))
		return CmdResult::FAILURE;

	// Check target exists
	auto u = ServerInstance->Users.FindUUID(parameters[0]);
	if (!u)
		return CmdResult::FAILURE;

	/* only join if it's local, otherwise just pass it on! */
	LocalUser* localuser = IS_LOCAL(u);
	if (localuser)
	{
		bool override = false;
		std::string key;
		if (parameters.size() >= 3)
		{
			key = parameters[2];
			if (key.empty())
				override = true;
		}

		Channel::JoinUser(localuser, parameters[1], override, key);
	}

	return CmdResult::SUCCESS;
}

RouteDescriptor CommandSVSJoin::GetRouting(User* user, const Params& parameters)
{
	return ROUTE_OPT_UCAST(parameters[0]);
}
