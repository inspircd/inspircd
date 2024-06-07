/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "utils.h"
#include "commands.h"

CmdResult CommandIdle::HandleRemote(RemoteUser* issuer, Params& params)
{
	/**
	 * There are two forms of IDLE: request and reply. Requests have one parameter,
	 * replies have more than one.
	 *
	 * If this is a request, 'issuer' did a /whois and its server wants to learn the
	 * idle time of the user in params[0].
	 *
	 * If this is a reply, params[0] is the user who did the whois and params.back() is
	 * the number of seconds 'issuer' has been idle.
	 */

	auto* target = ServerInstance->Users.FindUUID(params[0], true);
	if (!target)
		return CmdResult::FAILURE;

	LocalUser* localtarget = IS_LOCAL(target);
	if (!localtarget)
	{
		// Forward to target's server
		return CmdResult::SUCCESS;
	}

	if (params.size() >= 2)
	{
		ServerInstance->Parser.CallHandler("WHOIS", params, issuer);
	}
	else
	{
		// A server is asking us the idle time of our user
		unsigned int idle;
		if (localtarget->idle_lastmsg >= ServerInstance->Time())
			// Possible case when our clock ticked backwards
			idle = 0;
		else
			idle = ((unsigned int) (ServerInstance->Time() - localtarget->idle_lastmsg));

		CmdBuilder reply(target, "IDLE");
		reply.push(issuer->uuid);
		reply.push(ConvToStr(target->signon));
		reply.push(ConvToStr(idle));
		reply.Unicast(issuer);
	}

	return CmdResult::SUCCESS;
}
