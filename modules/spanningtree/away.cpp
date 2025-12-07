/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

CmdResult CommandAway::HandleRemote(::RemoteUser* u, Params& params)
{
	const auto prevstate = u->away;
	if (!params.empty())
	{
		time_t awaytime = params.size() > 1 ? ServerCommand::ExtractTS(params[0]) : 0;
		u->away.emplace(params.back(), awaytime);
		awayevprov.Call(&Away::EventListener::OnUserAway, u, prevstate);
	}
	else
	{
		u->away.reset();
		awayevprov.Call(&Away::EventListener::OnUserBack, u, prevstate);
	}
	return CmdResult::SUCCESS;
}

CommandAway::Builder::Builder(User* user)
	: CmdBuilder(user, "AWAY")
{
	if (user->IsAway())
		push_int(user->away->time).push_last(user->away->message);
}
