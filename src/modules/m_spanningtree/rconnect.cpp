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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

#include "main.h"
#include "utils.h"
#include "commands.h"

CommandRConnect::CommandRConnect (Module* Creator)
	: Command(Creator, "RCONNECT", 2)
{
	flags_needed = 'o';
	syntax = "<remote-server-mask> <target-server-mask>";
}

CmdResult CommandRConnect::Handle(User* user, const Params& parameters)
{
	/* First see if the server which is being asked to connect to another server in fact exists */
	if (!Utils->FindServerMask(parameters[0]))
	{
		user->WriteRemoteNotice(InspIRCd::Format("*** RCONNECT: Server \002%s\002 isn't connected to the network!", parameters[0].c_str()));
		return CMD_FAILURE;
	}

	/* Is this aimed at our server? */
	if (InspIRCd::Match(ServerInstance->Config->ServerName,parameters[0]))
	{
		/* Yes, initiate the given connect */
		ServerInstance->SNO->WriteToSnoMask('l',"Remote CONNECT from %s matching \002%s\002, connecting server \002%s\002",user->nick.c_str(),parameters[0].c_str(),parameters[1].c_str());
		CommandBase::Params para;
		para.push_back(parameters[1]);
		((ModuleSpanningTree*)(Module*)creator)->HandleConnect(para, user);
	}
	else
	{
		/* It's not aimed at our server, but if the request originates from our user
		 * acknowledge that we sent the request.
		 *
		 * It's possible that we're asking a server for something that makes no sense
		 * (e.g. connect to itself or to an already connected server), but we don't check
		 * for those conditions here, as ModuleSpanningTree::HandleConnect() (which will run
		 * on the target) does all the checking and error reporting.
		 */
		if (IS_LOCAL(user))
		{
			user->WriteNotice("*** RCONNECT: Sending remote connect to \002 " + parameters[0] + "\002 to connect server \002" + parameters[1] + "\002.");
		}
	}
	return CMD_SUCCESS;
}

RouteDescriptor CommandRConnect::GetRouting(User* user, const Params& parameters)
{
	return ROUTE_UNICAST(parameters[0]);
}
