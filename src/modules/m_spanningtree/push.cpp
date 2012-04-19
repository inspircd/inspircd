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
#include "socket.h"
#include "xline.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::Push(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 2)
		return true;
	User* u = ServerInstance->FindNick(params[0]);
	if (!u)
		return true;
	if (IS_LOCAL(u))
	{
		u->Write(params[1]);
	}
	else
	{
		// continue the raw onwards
		params[1] = ":" + params[1];
		Utils->DoOneToOne(prefix,"PUSH",params,u->server);
	}
	return true;
}

