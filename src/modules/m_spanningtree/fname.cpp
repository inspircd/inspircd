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
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


bool TreeSocket::ChangeName(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	User* u = this->ServerInstance->FindNick(prefix);
	if (u)
	{
		u->ChangeName(params[0].c_str());
		params[0] = ":" + params[0];
		Utils->DoOneToAllButSender(prefix,"FNAME",params,u->server);
	}
	return true;
}

