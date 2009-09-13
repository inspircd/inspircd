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

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


bool TreeSocket::OperQuit(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;

	User* u = this->ServerInstance->FindNick(prefix);

	if (u)
	{
		User::OperQuit.set(u, params[0]);
		params[0] = ":" + params[0];
		Utils->DoOneToAllButSender(prefix,"OPERQUIT",params,prefix);
	}
	return true;
}

