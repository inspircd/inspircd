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
#include "socket.h"
#include "xline.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::ServicePart(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 2)
		return true;

	std::string reason = "Services forced part";

	if (params.size() == 3)
		reason = params[2];

	User* u = ServerInstance->FindNick(params[0]);
	Channel* c = ServerInstance->FindChan(params[1]);

	if (u)
	{
		/* only part if it's local, otherwise just pass it on! */
		if (IS_LOCAL(u))
			c->PartUser(u, reason);
		Utils->DoOneToAllButSender(prefix,"SVSPART",params,prefix);
	}

	return true;
}

