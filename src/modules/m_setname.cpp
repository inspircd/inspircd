/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2005 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Provides support for the SETNAME command */



class CommandSetname : public Command
{
 public:
	CommandSetname (InspIRCd* Instance) : Command(Instance,"SETNAME", 0, 1, 1)
	{
		this->source = "m_setname.so";
		syntax = "<new-gecos>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		if (parameters.size() == 0)
		{
			user->WriteServ("NOTICE %s :*** SETNAME: GECOS must be specified", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (parameters[0].size() > ServerInstance->Config->Limits.MaxGecos)
		{
			user->WriteServ("NOTICE %s :*** SETNAME: GECOS too long", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (user->ChangeName(parameters[0].c_str()))
		{
			ServerInstance->SNO->WriteGlobalSno('a', "%s used SETNAME to change their GECOS to %s", user->nick.c_str(), parameters[0].c_str());
			return CMD_SUCCESS;
		}

		return CMD_LOCALONLY;
	}
};


class ModuleSetName : public Module
{
	CommandSetname*	mycommand;
 public:
	ModuleSetName(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSetname(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSetName()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSetName)
