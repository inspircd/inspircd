/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

int ModuleSpanningTree::HandleAdmin(const std::vector<std::string>& parameters, User* user)
{
	if (parameters.size() > 0)
	{
		if (InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0]))
			return 0;

		/* Remote ADMIN, the server is within the 1st parameter */
		std::deque<std::string> params;
		params.push_back(parameters[0]);
		/* Send it out remotely, generate no reply yet */
		TreeServer* s = Utils->FindServerMask(parameters[0]);
		if (s)
		{
			params[0] = s->GetName();
			Utils->DoOneToOne(user->uuid, "ADMIN", params, s->GetName());
		}
		else
			user->WriteNumeric(ERR_NOSUCHSERVER, "%s %s :No such server", user->nick.c_str(), parameters[0].c_str());
		return 1;
	}
	return 0;
}

