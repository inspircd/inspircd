/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
#include "wildcard.h"
#include "xline.h"
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

/** Because Andy insists that services-compatible servers must
 * implement SVSNICK and SVSJOIN, that's exactly what we do :p
 */
bool TreeSocket::ForceNick(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 3)
		return true;

	User* u = this->Instance->FindNick(params[0]);

	if (u)
	{
		Utils->DoOneToAllButSender(prefix,"SVSNICK",params,prefix);

		if (IS_LOCAL(u))
		{
			std::deque<std::string> par;
			par.push_back(params[1]);

			if (!u->ForceNickChange(params[1].c_str()))
			{
				/* buh. UID them */
				if (!u->ForceNickChange(u->uuid))
				{
					this->Instance->Users->QuitUser(u, "Nickname collision");
					return true;
				}
			}

			u->age = atoi(params[2].c_str());
		}
	}

	return true;
}

