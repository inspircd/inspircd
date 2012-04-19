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

int ModuleSpanningTree::HandleRemoteWhois(const std::vector<std::string>& parameters, User* user)
{
	if ((IS_LOCAL(user)) && (parameters.size() > 1))
	{
		User* remote = ServerInstance->FindNick(parameters[1]);
		if ((remote) && (remote->GetFd() < 0))
		{
			std::deque<std::string> params;
			params.push_back(remote->uuid);
			Utils->DoOneToOne(user->uuid,"IDLE",params,remote->server);
			return 1;
		}
		else if (!remote)
		{
			user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[1].c_str());
			user->WriteNumeric(318, "%s %s :End of /WHOIS list.",user->nick.c_str(), parameters[1].c_str());
			return 1;
		}
	}
	return 0;
}

