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

int ModuleSpanningTree::OnPreCommand(const std::string &command, const char** parameters, int pcnt, User *user, bool validated, const std::string &original_line)
{
	/* If the command doesnt appear to be valid, we dont want to mess with it. */
	if (!validated)
		return 0;

	if (command == "CONNECT")
	{
		return this->HandleConnect(parameters,pcnt,user);
	}
	else if (command == "STATS")
	{
		return this->HandleStats(parameters,pcnt,user);
	}
	else if (command == "MOTD")
	{
		return this->HandleMotd(parameters,pcnt,user);
	}
	else if (command == "ADMIN")
	{
		return this->HandleAdmin(parameters,pcnt,user);
	}
	else if (command == "SQUIT")
	{
		return this->HandleSquit(parameters,pcnt,user);
	}
	else if (command == "MAP")
	{
		this->HandleMap(parameters,pcnt,user);
		return 1;
	}
	else if ((command == "TIME") && (pcnt > 0))
	{
		return this->HandleTime(parameters,pcnt,user);
	}
	else if (command == "LUSERS")
	{
		this->HandleLusers(parameters,pcnt,user);
		return 1;
	}
	else if (command == "LINKS")
	{
		this->HandleLinks(parameters,pcnt,user);
		return 1;
	}
	else if (command == "WHOIS")
	{
		if (pcnt > 1)
		{
			// remote whois
			return this->HandleRemoteWhois(parameters,pcnt,user);
		}
	}
	else if ((command == "VERSION") && (pcnt > 0))
	{
		this->HandleVersion(parameters,pcnt,user);
		return 1;
	}
	else if ((command == "MODULES") && (pcnt > 0))
	{
		return this->HandleModules(parameters,pcnt,user);
	}
	return 0;
}

