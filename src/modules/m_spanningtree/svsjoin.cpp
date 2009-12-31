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
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::ServiceJoin(const std::string &prefix, std::deque<std::string> &params)
{
	// Check params
	if (params.size() < 2)
		return true;

	// Check for valid channel name
	if (!ServerInstance->IsChannel(params[1].c_str(), ServerInstance->Config->Limits.ChanMax))
		return true;

	// Check target exists
	User* u = this->ServerInstance->FindNick(params[0]);
	if (!u)
		return true;

	/* only join if it's local, otherwise just pass it on! */
	if (IS_LOCAL(u))
		Channel::JoinUser(this->ServerInstance, u, params[1].c_str(), false, "", false, ServerInstance->Time());
	Utils->DoOneToAllButSender(prefix,"SVSJOIN",params,prefix);
	return true;
}

