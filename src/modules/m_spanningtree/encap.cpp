/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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

#include "commands.h"
#include "main.h"

/** ENCAP */
CmdResult CommandEncap::Handle(User* user, std::vector<std::string>& params)
{
	if (ServerInstance->Config->GetSID() == params[0] || InspIRCd::Match(ServerInstance->Config->ServerName, params[0]))
	{
		parameterlist plist(params.begin() + 2, params.end());

		// XXX: Workaround for SVS* commands provided by spanningtree not being registered in the core
		if ((params[1] == "SVSNICK") || (params[1] == "SVSJOIN") || (params[1] == "SVSPART"))
		{
			ServerCommand* const scmd = Utils->Creator->CmdManager.GetHandler(params[1]);
			if (scmd)
				scmd->Handle(user, plist);
			return CMD_SUCCESS;
		}

		Command* cmd = NULL;
		ServerInstance->Parser.CallHandler(params[1], plist, user, &cmd);
		// Discard return value, ENCAP shall succeed even if the command does not exist

		if ((cmd) && (cmd->force_manual_route))
			return CMD_FAILURE;
	}
	return CMD_SUCCESS;
}

RouteDescriptor CommandEncap::GetRouting(User* user, const std::vector<std::string>& params)
{
	if (params[0].find_first_of("*?") != std::string::npos)
		return ROUTE_BROADCAST;
	return ROUTE_UNICAST(params[0]);
}
