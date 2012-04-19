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

