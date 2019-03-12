/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

#include "commands.h"

CmdResult CommandSVSPart::Handle(User* user, Params& parameters)
{
	User* u = ServerInstance->FindUUID(parameters[0]);
	if (!u)
		return CMD_FAILURE;

	Channel* c = ServerInstance->FindChan(parameters[1]);
	if (!c)
		return CMD_FAILURE;

	if (IS_LOCAL(u))
	{
		std::string reason = (parameters.size() == 3) ? parameters[2] : "Services forced part";
		c->PartUser(u, reason);
	}
	return CMD_SUCCESS;
}

RouteDescriptor CommandSVSPart::GetRouting(User* user, const Params& parameters)
{
	return ROUTE_OPT_UCAST(parameters[0]);
}
