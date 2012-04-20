/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

