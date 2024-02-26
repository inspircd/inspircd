/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
CmdResult CommandEncap::Handle(User* user, Params& params)
{
	if (ServerInstance->Config->ServerId == params[0] || InspIRCd::Match(ServerInstance->Config->ServerName, params[0]))
	{
		CommandBase::Params plist(params.begin() + 2, params.end());
		Command* cmd = nullptr;
		ServerInstance->Parser.CallHandler(params[1], plist, user, &cmd);
		// Discard return value, ENCAP shall succeed even if the command does not exist

		if ((cmd) && (cmd->force_manual_route))
			return CmdResult::FAILURE;
	}
	return CmdResult::SUCCESS;
}

RouteDescriptor CommandEncap::GetRouting(User* user, const Params& params)
{
	if (params[0].find_first_of("*?") != std::string::npos)
		return ROUTE_BROADCAST;
	return ROUTE_UNICAST(params[0]);
}
