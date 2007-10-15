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

int ModuleSpanningTree::HandleAdmin(const char** parameters, int pcnt, User* user)
{
	if (pcnt > 0)
	{
		if (match(ServerInstance->Config->ServerName, parameters[0]))
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
			user->WriteServ( "402 %s %s :No such server", user->nick, parameters[0]);
		return 1;
	}
	return 0;
}

