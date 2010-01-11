/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

ModResult ModuleSpanningTree::HandleRemoteWhois(const std::vector<std::string>& parameters, User* user)
{
	if ((IS_LOCAL(user)) && (parameters.size() > 1))
	{
		User* remote = ServerInstance->FindNick(parameters[1]);
		if (remote && !IS_LOCAL(remote))
		{
			parameterlist params;
			params.push_back(remote->uuid);
			Utils->DoOneToOne(user->uuid,"IDLE",params,remote->server);
			return MOD_RES_DENY;
		}
		else if (!remote)
		{
			user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[1].c_str());
			user->WriteNumeric(318, "%s %s :End of /WHOIS list.",user->nick.c_str(), parameters[1].c_str());
			return MOD_RES_DENY;
		}
	}
	return MOD_RES_PASSTHRU;
}

