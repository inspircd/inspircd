/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

/** Because Andy insists that services-compatible servers must
 * implement SVSNICK and SVSJOIN, that's exactly what we do :p
 */
bool TreeSocket::ForceNick(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 3)
		return true;

	User* u = this->ServerInstance->FindNick(params[0]);

	if (u)
	{
		Utils->DoOneToAllButSender(prefix,"SVSNICK",params,prefix);

		if (IS_LOCAL(u))
		{
			std::string nick = params[1];
			if (isdigit(nick[0]))
				nick = u->uuid;

			if (!u->ForceNickChange(nick.c_str()))
			{
				/* buh. UID them */
				if (!u->ForceNickChange(u->uuid.c_str()))
				{
					this->ServerInstance->Users->QuitUser(u, "Nickname collision");
					return true;
				}
			}

			u->age = atoi(params[2].c_str());
		}
	}

	return true;
}

