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
#include "treeserver.h"
#include "treesocket.h"

bool TreeSocket::LocalPong(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;

	const std::string& forwardto = params[0];
	if (forwardto == ServerInstance->Config->GetSID())
	{
		// PONG for us
		TreeServer* ServerSource = Utils->FindServer(prefix);
		if (ServerSource)
		{
			long ts = ServerInstance->Time() * 1000 + (ServerInstance->Time_ns() / 1000000);
			ServerSource->rtt = ts - ServerSource->LastPingMsec;
			ServerSource->SetPingFlag();
		}
	}
	else
	{
		// not for us, pass it on :)
		Utils->DoOneToOne(prefix,"PONG",params,forwardto);
	}
	return true;
}

