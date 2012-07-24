/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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


/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

ModResult ModuleSpanningTree::HandleRemoteWhois(const std::vector<std::string>& parameters, User* user)
{
	if ((IS_LOCAL(user)) && (parameters.size() > 1))
	{
		User* remote = ServerInstance->FindNickOnly(parameters[1]);
		if (remote && !IS_LOCAL(remote))
		{
			parameterlist params;
			params.push_back(remote->uuid);
			Utils->DoOneToOne(user->uuid,"IDLE",params,remote->server);
			return MOD_RES_DENY;
		}
		else if (!remote)
		{
			user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[1].c_str());
			user->WriteNumeric(318, "%s %s :End of /WHOIS list.",user->nick.c_str(), parameters[1].c_str());
			return MOD_RES_DENY;
		}
	}
	return MOD_RES_PASSTHRU;
}

