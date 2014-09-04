/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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
#include "core_user.h"

CommandMode::CommandMode(Module* parent)
	: Command(parent, "MODE", 1)
{
	syntax = "<target> <modes> {<mode-parameters>}";
}

CmdResult CommandMode::Handle(const std::vector<std::string>& parameters, User* user)
{
	ServerInstance->Modes->Process(parameters, user, (IS_LOCAL(user) ? ModeParser::MODE_NONE : ModeParser::MODE_LOCALONLY));
	return CMD_SUCCESS;
}

RouteDescriptor CommandMode::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
