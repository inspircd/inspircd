/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

enum
{
	// From RFC 1459.
	RPL_UNAWAY = 305,
	RPL_NOWAWAY = 306
};

CommandAway::CommandAway(Module* parent)
	: Command(parent, "AWAY", 0, 1)
	, awayevprov(parent)
{
	syntax = { "[:<message>]" };
	works_before_reg = true;
}

CmdResult CommandAway::Handle(User* user, const Params& parameters)
{
	LocalUser* luser = IS_LOCAL(user);
	if (!parameters.empty())
	{
		std::string message(parameters[0]);
		if (luser)
		{
			ModResult res = awayevprov.FirstResult(&Away::EventListener::OnUserPreAway, luser, message);
			if (res == MOD_RES_DENY)
				return CmdResult::FAILURE;
		}

		const auto prevstate = user->away;
		user->away.emplace(message);
		user->WriteNumeric(RPL_NOWAWAY, "You have been marked as being away");
		awayevprov.Call(&Away::EventListener::OnUserAway, user, prevstate);
	}
	else
	{
		if (luser)
		{
			ModResult res = awayevprov.FirstResult(&Away::EventListener::OnUserPreBack, luser);
			if (res == MOD_RES_DENY)
				return CmdResult::FAILURE;
		}

		const auto prevstate = user->away;
		user->away.reset();
		user->WriteNumeric(RPL_UNAWAY, "You are no longer marked as being away");
		awayevprov.Call(&Away::EventListener::OnUserBack, user, prevstate);
	}

	return CmdResult::SUCCESS;
}

RouteDescriptor CommandAway::GetRouting(User* user, const Params& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
