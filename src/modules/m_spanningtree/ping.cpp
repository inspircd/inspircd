/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "treesocket.h"

bool TreeSocket::LocalPing(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;

	const std::string& forwardto = params[0];
	if (forwardto == ServerInstance->Config->GetSID())
	{
		// PING for us, reply with a PONG
		std::string reply = ":" + forwardto + " PONG " + prefix;
		if (params.size() >= 2)
			// If there is a second parameter, append it
			reply.append(" :").append(params[1]);

		this->WriteLine(reply);
	}
	else
	{
		// not for us, pass it on :)
		Utils->DoOneToOne(prefix,"PING",params,forwardto);
	}
	return true;
}


