/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
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
#include "../transport.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

int ModuleSpanningTree::HandleTime(const std::vector<std::string>& parameters, User* user)
{
	if ((IS_LOCAL(user)) && (parameters.size() > 0))
	{
		TreeServer* found = Utils->FindServerMask(parameters[0].c_str());
		if (found)
		{
			// we dont' override for local server
			if (found == Utils->TreeRoot)
				return 0;

			std::deque<std::string> params;
			params.push_back(found->GetName());
			params.push_back(user->uuid);
			Utils->DoOneToOne(ServerInstance->Config->GetSID(),"TIME",params,found->GetName());
		}
		else
		{
			user->WriteNumeric(ERR_NOSUCHSERVER, "%s %s :No such server",user->nick.c_str(),parameters[0].c_str());
		}
	}
	return 1;
}

