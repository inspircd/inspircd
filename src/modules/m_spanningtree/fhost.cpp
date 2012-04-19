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
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


bool TreeSocket::ChangeHost(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	User* u = this->ServerInstance->FindNick(prefix);

	if (u)
	{
		u->ChangeDisplayedHost(params[0].c_str());
		Utils->DoOneToAllButSender(prefix,"FHOST",params,u->server);
	}
	return true;
}

