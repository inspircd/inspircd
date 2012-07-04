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
#include "socket.h"
#include "xline.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

bool TreeSocket::Whois(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;

	/* If this is a request, this user did the /whois
	 * If this is a reply, this user's information is in params[1] and params[2]
	 */
	User* issuer = ServerInstance->FindUUID(prefix);
	if ((!issuer) || (IS_SERVER(issuer)))
		return true;

	/* If this is a request, this is the user whose idle information was requested
	 * If this is a reply, this user did the /whois
	 */
	User* target = ServerInstance->FindUUID(params[0]);
	if ((!target) || (IS_SERVER(target)))
		return true;

	LocalUser* localtarget = IS_LOCAL(target);
	if (!localtarget)
	{
		// Forward to target's server
		Utils->DoOneToOne(prefix, "IDLE", params, target->server);
		return true;
	}

	if (params.size() >= 2)
	{
		ServerInstance->Parser->CallHandler("WHOIS", params, issuer);
	}
	else
	{
		// A server is asking us the idle time of our user
		unsigned int idle;
		if (localtarget->idle_lastmsg >= ServerInstance->Time())
			// Possible case when our clock ticked backwards
			idle = 0;
		else
			idle = ((unsigned int) (localtarget->idle_lastmsg - ServerInstance->Time()));

		parameterlist reply;
		reply.push_back(prefix);
		reply.push_back(ConvToStr(target->signon));
		reply.push_back(ConvToStr(idle));
		Utils->DoOneToOne(params[0], "IDLE", reply, issuer->server);
	}

	return true;
}
