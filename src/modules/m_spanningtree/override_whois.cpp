/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
#include "wildcard.h"
#include "xline.h"      
#include "transport.h"  
			
#include "m_spanningtree/timesynctimer.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/rconnect.h"
#include "m_spanningtree/rsquit.h"
	
/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_spanningtree/rconnect.h m_spanningtree/rsquit.h */

int ModuleSpanningTree::HandleRemoteWhois(const char** parameters, int pcnt, User* user)
{
	if ((IS_LOCAL(user)) && (pcnt > 1))
	{
		User* remote = ServerInstance->FindNick(parameters[1]);
		if ((remote) && (remote->GetFd() < 0))
		{
			std::deque<std::string> params;
			params.push_back(parameters[1]);
			Utils->DoOneToOne(user->uuid,"IDLE",params,remote->server);
			return 1;
		}
		else if (!remote)
		{
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[1]);
			user->WriteServ("318 %s %s :End of /WHOIS list.",user->nick, parameters[1]);
			return 1;
		}
	}
	return 0;
}

