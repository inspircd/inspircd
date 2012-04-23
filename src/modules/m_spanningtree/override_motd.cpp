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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

int ModuleSpanningTree::HandleMotd(const std::vector<std::string>& parameters, User* user)
{
	if (parameters.size() > 0)
	{
		if (InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0]))
			return 0;

		/* Remote MOTD, the server is within the 1st parameter */
		std::deque<std::string> params;
		params.push_back(parameters[0]);
		/* Send it out remotely, generate no reply yet */
		TreeServer* s = Utils->FindServerMask(parameters[0]);
		if (s)
		{
			params[0] = s->GetName();
			Utils->DoOneToOne(user->uuid, "MOTD", params, s->GetName());
		}
		else
			user->WriteNumeric(ERR_NOSUCHSERVER, "%s %s :No such server", user->nick.c_str(), parameters[0].c_str());
		return 1;
	}
	return 0;
}

