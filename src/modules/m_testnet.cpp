/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

class CommandTest : public Command
{
 public:
	CommandTest(Module* parent) : Command(parent, "TEST", 1)
	{
		syntax = "<action> <parameters>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		if (parameters[0] == "flood")
		{
			unsigned int count = parameters.size() > 1 ? atoi(parameters[1].c_str()) : 100;
			std::string line = parameters.size() > 2 ? parameters[2] : ":z.z NOTICE !flood :Flood text";
			for(unsigned int i=0; i < count; i++)
				user->Write(line);
		}
		else if (parameters[0] == "freeze" && IS_LOCAL(user) && parameters.size() > 1)
		{
			IS_LOCAL(user)->CommandFloodPenalty += atoi(parameters[1].c_str());
		}
		return CMD_SUCCESS;
	}
};

class ModuleTest : public Module
{
	CommandTest cmd;
 public:
	ModuleTest() : cmd(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		if (!strstr(ServerInstance->Config->ServerName.c_str(), ".test"))
			throw ModuleException("Don't load modules without reading their descriptions!");
		ServerInstance->Modules->AddService(cmd);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides a module for testing the server while linked in a network", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleTest)
