/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
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
#include "servercommand.h"

ServerCommand::ServerCommand(Module* Creator, const std::string& Name, unsigned int MinParams, unsigned int MaxParams)
	: CommandBase(Creator, Name, MinParams, MaxParams)
{
}

void ServerCommand::RegisterService()
{
	auto* st = static_cast<ModuleSpanningTree*>(creator.ptr());
	st->CmdManager.AddCommand(this);
}

RouteDescriptor ServerCommand::GetRouting(User* user, const Params& parameters)
{
	// Broadcast server-to-server commands unless overridden
	return ROUTE_BROADCAST;
}

time_t ServerCommand::ExtractTS(const std::string& tsstr)
{
	time_t ts = ConvToNum<time_t>(tsstr);
	if (ts <= 0)
		throw ProtocolException("Invalid TS: " + tsstr);
	return ts;
}

ServerCommand* ServerCommandManager::GetHandler(const std::string& command) const
{
	ServerCommandMap::const_iterator it = commands.find(command);
	if (it != commands.end())
		return it->second;
	return nullptr;
}

bool ServerCommandManager::AddCommand(ServerCommand* cmd)
{
	return commands.emplace(cmd->name, cmd).second;
}
