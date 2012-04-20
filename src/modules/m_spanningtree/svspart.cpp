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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::ServicePart(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 2)
		return true;

	std::string reason = "Services forced part";

	if (params.size() == 3)
		reason = params[2];

	User* u = this->ServerInstance->FindNick(params[0]);
	Channel* c = this->ServerInstance->FindChan(params[1]);

	if (u)
	{
		/* only part if it's local, otherwise just pass it on! */
		if (IS_LOCAL(u))
			if (!c->PartUser(u, reason))
				delete c;
		Utils->DoOneToAllButSender(prefix,"SVSPART",params,prefix);
	}

	return true;
}

