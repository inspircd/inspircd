/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
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


bool TreeSocket::DelLine(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 2)
		return true;

	std::string setter = "<unknown>";

	User* user = ServerInstance->FindNick(prefix);
	if (user)
		setter = user->nick;
	else
	{
		TreeServer* t = Utils->FindServer(prefix);
		if (t)
			setter = t->GetName().c_str();
	}


	/* NOTE: No check needed on 'user', this function safely handles NULL */
	if (ServerInstance->XLines->DelLine(params[1].c_str(), params[0], user))
	{
		ServerInstance->SNO->WriteToSnoMask('X',"%s removed %s%s on %s", setter.c_str(),
				params[0].c_str(), params[0].length() == 1 ? "-line" : "", params[1].c_str());
		Utils->DoOneToAllButSender(prefix,"DELLINE", params, prefix);
	}
	return true;
}

