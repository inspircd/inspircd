/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018, 2020, 2023-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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

#include "main.h"
#include "utils.h"
#include "commands.h"

CommandRConnect::CommandRConnect (Module* Creator)
	: Command(Creator, "RCONNECT", 2)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<remote-server-mask> <target-server-mask>" };
}

CmdResult CommandRConnect::Handle(User* user, const Params& parameters)
{
	/* First see if the server which is being asked to connect to another server in fact exists */
	if (!Utils->FindServerMask(parameters[0]))
	{
		user->WriteRemoteNotice("*** RCONNECT: Server \002{}\002 isn't connected to the network!", parameters[0]);
		return CmdResult::FAILURE;
	}

	/* Is this aimed at our server? */
	if (InspIRCd::Match(ServerInstance->Config->ServerName, parameters[0]))
	{
		/* Yes, initiate the given connect */
		ServerInstance->SNO.WriteToSnoMask('l', "Remote CONNECT from {} matching \002{}\002, connecting server \002{}\002",
			user->nick, parameters[0], parameters[1]);
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
	return CmdResult::SUCCESS;
}

RouteDescriptor CommandRConnect::GetRouting(User* user, const Params& parameters)
{
	return ROUTE_UNICAST(parameters[0]);
}
