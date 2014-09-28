/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "core_info.h"

CommandTime::CommandTime(Module* parent)
	: Command(parent, "TIME", 0, 0)
{
	syntax = "[<servername>]";
}

CmdResult CommandTime::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 0 && parameters[0] != ServerInstance->Config->ServerName)
		return CMD_SUCCESS;

	user->SendText(":%s %03d %s %s :%s", ServerInstance->Config->ServerName.c_str(), RPL_TIME, user->nick.c_str(),
		ServerInstance->Config->ServerName.c_str(), InspIRCd::TimeString(ServerInstance->Time()).c_str());

	return CMD_SUCCESS;
}

RouteDescriptor CommandTime::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	if (parameters.size() > 0)
		return ROUTE_UNICAST(parameters[0]);
	return ROUTE_LOCALONLY;
}
