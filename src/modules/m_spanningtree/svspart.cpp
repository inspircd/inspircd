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

	User* u = this->ServerInstance->FindNick(params[0]);
	Channel* c = this->ServerInstance->FindChan(params[1]);

	if (u && c)
	{
		/* only part if it's local, otherwise just pass it on! */
		if (IS_LOCAL(u))
		{
			std::string reason;
			reason = (params.size() == 3) ? params[2] : "Services forced part";
			if (!c->PartUser(u, reason))
				delete c;
		}
		else
		{
			/* Only forward when the route to the target is not the same as the sender.
			 * This occurs with 1.2.9 and older servers, as they broadcast SVSJOIN/SVSPART,
			 * so we can end up here with a user who is reachable via the sender.
			 * If that's the case, just drop the command.
			 */
			TreeServer* routeserver = Utils->BestRouteTo(u->server);
			if ((routeserver) && (routeserver->GetSocket() != this))
				Utils->DoOneToOne(prefix,"SVSPART",params,u->server);
		}
	}

	return true;
}

