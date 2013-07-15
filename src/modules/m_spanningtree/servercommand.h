/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
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


#pragma once

class ServerCommand : public CommandBase
{
 public:
	ServerCommand(Module* Creator, const std::string& Name, unsigned int MinPara = 0, unsigned int MaxPara = 0);

	virtual CmdResult Handle(User* user, std::vector<std::string>& parameters) = 0;
	virtual RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
};

class ServerCommandManager
{
	typedef TR1NS::unordered_map<std::string, ServerCommand*> ServerCommandMap;
	ServerCommandMap commands;

 public:
	ServerCommand* GetHandler(const std::string& command) const;
	bool AddCommand(ServerCommand* cmd);
};
