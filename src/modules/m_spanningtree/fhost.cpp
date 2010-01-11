/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


bool TreeSocket::ChangeHost(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;
	User* u = ServerInstance->FindNick(prefix);

	if (u)
	{
		u->ChangeDisplayedHost(params[0].c_str());
		Utils->DoOneToAllButSender(prefix,"FHOST",params,u->server);
	}
	return true;
}

