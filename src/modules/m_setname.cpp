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



class CommandSetname : public Command
{
 public:
	CommandSetname(Module* Creator) : Command(Creator,"SETNAME", 1, 1)
	{
		allow_empty_last_param = false;
		syntax = "<newname>";
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (parameters[0].size() > ServerInstance->Config->Limits.MaxReal)
		{
			user->WriteNotice("*** SETNAME: Real name is too long");
			return CMD_FAILURE;
		}

		if (user->ChangeName(parameters[0]))
		{
			ServerInstance->SNO->WriteGlobalSno('a', "%s used SETNAME to change their real name to '%s'", user->nick.c_str(), parameters[0].c_str());
		}

		return CMD_SUCCESS;
	}
};


class ModuleSetName : public Module
{
	CommandSetname cmd;
 public:
	ModuleSetName()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for the SETNAME command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSetName)
