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

bool TreeSocket::ServiceJoin(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 2)
		return true;

	if (!this->Instance->IsChannel(params[1].c_str()))
		return true;

	User* u = this->Instance->FindNick(params[0]);

	if (u)
	{
		/* only join if it's local, otherwise just pass it on! */
		if (IS_LOCAL(u))
			Channel::JoinUser(this->Instance, u, params[1].c_str(), false, "", false, Instance->Time());
		Utils->DoOneToAllButSender(prefix,"SVSJOIN",params,prefix);
	}
	return true;
}

