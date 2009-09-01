/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "../transport.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

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


