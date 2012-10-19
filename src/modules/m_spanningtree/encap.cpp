/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/** ENCAP */
void TreeSocket::Encap(User* who, parameterlist &params)
{
	if (params.size() > 1)
	{
		if (ServerInstance->Config->GetSID() == params[0] || InspIRCd::Match(ServerInstance->Config->ServerName, params[0]))
		{
			parameterlist plist(params.begin() + 2, params.end());
			ServerInstance->Parser->CallHandler(params[1], plist, who);
			// discard return value, ENCAP shall succeed even if the command does not exist
		}
		
		params[params.size() - 1] = ":" + params[params.size() - 1];

		if (params[0].find_first_of("*?") != std::string::npos)
		{
			Utils->DoOneToAllButSender(who->uuid, "ENCAP", params, who->server);
		}
		else
			Utils->DoOneToOne(who->uuid, "ENCAP", params, params[0]);
	}
}

