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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "commands.h"

CmdResult CommandSVSPart::Handle(const std::vector<std::string>& parameters, User *user)
{
	std::string reason = "Services forced part";

	if (parameters.size() == 3)
		reason = parameters[2];

	User* u = ServerInstance->FindNick(parameters[0]);
	Channel* c = ServerInstance->FindChan(parameters[1]);

	if (u && IS_LOCAL(u))
		c->PartUser(u, reason);

	return CMD_SUCCESS;
}

RouteDescriptor CommandSVSPart::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	User* u = ServerInstance->FindNick(parameters[0]);
	if (u)
		return ROUTE_OPT_UCAST(u->server);
	return ROUTE_LOCALONLY;
}
