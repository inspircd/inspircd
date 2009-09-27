/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a module for testing the server while linked in a network */

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
		else if (parameters[0] == "freeze")
		{
			user->Penalty += 100;
		}
		else if (parameters[0] == "shutdown")
		{
			int i = parameters.size() > 1 ? atoi(parameters[1].c_str()) : 2;
			ServerInstance->SE->Shutdown(user->GetFd(), i);
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
		if (!strstr(ServerInstance->Config->ServerName, ".test"))
			throw ModuleException("Don't load modules without reading their descriptions!");
		ServerInstance->AddCommand(&cmd);
	}

	Version GetVersion()
	{
		return Version("Provides a module for testing the server while linked in a network", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleTest)

