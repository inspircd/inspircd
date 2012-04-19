/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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


#include "inspircd.h"
#include "socket.h"
#include "xline.h"

#include "resolvers.h"
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "commands.h"

CommandRConnect::CommandRConnect (Module* Creator, SpanningTreeUtilities* Util)
	: Command(Creator, "RCONNECT", 2), Utils(Util)
{
	flags_needed = 'o';
	syntax = "<remote-server-mask> <target-server-mask>";
}

CmdResult CommandRConnect::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (IS_LOCAL(user))
	{
		if (!Utils->FindServerMask(parameters[0]))
		{
			user->WriteServ("NOTICE %s :*** RCONNECT: Server \002%s\002 isn't connected to the network!", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		user->WriteServ("NOTICE %s :*** RCONNECT: Sending remote connect to \002%s\002 to connect server \002%s\002.",user->nick.c_str(),parameters[0].c_str(),parameters[1].c_str());
	}

	/* Is this aimed at our server? */
	if (InspIRCd::Match(ServerInstance->Config->ServerName,parameters[0]))
	{
		/* Yes, initiate the given connect */
		ServerInstance->SNO->WriteToSnoMask('l',"Remote CONNECT from %s matching \002%s\002, connecting server \002%s\002",user->nick.c_str(),parameters[0].c_str(),parameters[1].c_str());
		std::vector<std::string> para;
		para.push_back(parameters[1]);
		((ModuleSpanningTree*)(Module*)creator)->HandleConnect(para, user);
	}
	return CMD_SUCCESS;
}

RouteDescriptor CommandRConnect::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return ROUTE_UNICAST(parameters[0]);
}
