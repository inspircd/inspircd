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
#include "transport.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

int ModuleSpanningTree::OnPreCommand(std::string &command, std::vector<std::string>& parameters, User *user, bool validated, const std::string &original_line)
{
	/* If the command doesnt appear to be valid, we dont want to mess with it. */
	if (!validated)
		return 0;

	if (command == "CONNECT")
	{
		return this->HandleConnect(parameters,user);
	}
	else if (command == "STATS")
	{
		return this->HandleStats(parameters,user);
	}
	else if (command == "MOTD")
	{
		return this->HandleMotd(parameters,user);
	}
	else if (command == "ADMIN")
	{
		return this->HandleAdmin(parameters,user);
	}
	else if (command == "SQUIT")
	{
		return this->HandleSquit(parameters,user);
	}
	else if (command == "MAP")
	{
		return this->HandleMap(parameters,user);
	}
	else if ((command == "TIME") && (parameters.size() > 0))
	{
		return this->HandleTime(parameters,user);
	}
	else if (command == "LUSERS")
	{
		this->HandleLusers(parameters,user);
		return 1;
	}
	else if (command == "LINKS")
	{
		this->HandleLinks(parameters,user);
		return 1;
	}
	else if (command == "WHOIS")
	{
		if (parameters.size() > 1)
		{
			// remote whois
			return this->HandleRemoteWhois(parameters,user);
		}
	}
	else if ((command == "VERSION") && (parameters.size() > 0))
	{
		this->HandleVersion(parameters,user);
		return 1;
	}
	else if ((command == "MODULES") && (parameters.size() > 0))
	{
		return this->HandleModules(parameters,user);
	}
	return 0;
}

