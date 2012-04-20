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

int ModuleSpanningTree::HandleTime(const std::vector<std::string>& parameters, User* user)
{
	if ((IS_LOCAL(user)) && (parameters.size() > 0))
	{
		TreeServer* found = Utils->FindServerMask(parameters[0].c_str());
		if (found)
		{
			// we dont' override for local server
			if (found == Utils->TreeRoot)
				return 0;

			std::deque<std::string> params;
			params.push_back(found->GetName());
			params.push_back(user->uuid);
			Utils->DoOneToOne(ServerInstance->Config->GetSID(),"TIME",params,found->GetName());
		}
		else
		{
			user->WriteNumeric(ERR_NOSUCHSERVER, "%s %s :No such server",user->nick.c_str(),parameters[0].c_str());
		}
	}
	return 1;
}

