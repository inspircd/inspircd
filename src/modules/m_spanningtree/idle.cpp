/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Adam <Adam@anope.org>
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

CmdResult CommandIdle::HandleRemote(RemoteUser* issuer, std::vector<std::string>& params)
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

	User* target = ServerInstance->FindUUID(params[0]);
	if ((!target) || (target->registered != REG_ALL))
		return CMD_FAILURE;

	LocalUser* localtarget = IS_LOCAL(target);
	if (!localtarget)
	{
		// Forward to target's server
		return CMD_SUCCESS;
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

		CmdBuilder reply(params[0], "IDLE");
		reply.push_back(issuer->uuid);
		reply.push_back(ConvToStr(target->signon));
		reply.push_back(ConvToStr(idle));
		reply.Unicast(issuer);
	}

	return CMD_SUCCESS;
}
