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

bool TreeSocket::Whois(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	User* u = this->ServerInstance->FindNick(prefix);
	if (u)
	{
		// an incoming request
		if (params.size() == 1)
		{
			User* x = this->ServerInstance->FindNick(params[0]);
			if ((x) && (IS_LOCAL(x)))
			{
				char signon[MAXBUF];
				char idle[MAXBUF];
				snprintf(signon, MAXBUF, "%lu", (unsigned long)x->signon);
				snprintf(idle, MAXBUF, "%lu", (unsigned long)abs((long)((x->idle_lastmsg) - ServerInstance->Time())));
				std::deque<std::string> par;
				par.push_back(prefix);
				par.push_back(signon);
				par.push_back(idle);
				// ours, we're done, pass it BACK
				Utils->DoOneToOne(params[0], "IDLE", par, u->server);
			}
			else
			{
				// not ours pass it on
				if (x)
					Utils->DoOneToOne(prefix, "IDLE", params, x->server);
			}
		}
		else if (params.size() == 3)
		{
			std::string who_did_the_whois = params[0];
			User* who_to_send_to = this->ServerInstance->FindNick(who_did_the_whois);
			if ((who_to_send_to) && (IS_LOCAL(who_to_send_to)))
			{
				// an incoming reply to a whois we sent out
				std::string nick_whoised = prefix;
				unsigned long signon = atoi(params[1].c_str());
				unsigned long idle = atoi(params[2].c_str());
				if ((who_to_send_to) && (IS_LOCAL(who_to_send_to)))
				{
					do_whois(this->ServerInstance, who_to_send_to, u, signon, idle, nick_whoised.c_str());
				}
			}
			else
			{
				// not ours, pass it on
				if (who_to_send_to)
					Utils->DoOneToOne(prefix, "IDLE", params, who_to_send_to->server);
			}
		}
	}
	return true;
}


