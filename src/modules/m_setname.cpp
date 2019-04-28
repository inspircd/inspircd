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
	bool notifyopers;
	CommandSetname(Module* Creator) : Command(Creator,"SETNAME", 1, 1)
	{
		allow_empty_last_param = false;
		syntax = ":<realname>";
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (parameters[0].size() > ServerInstance->Config->Limits.MaxReal)
		{
			user->WriteNotice("*** SETNAME: Real name is too long");
			return CMD_FAILURE;
		}

		if (user->ChangeRealName(parameters[0]))
		{
			if (notifyopers)
				ServerInstance->SNO->WriteGlobalSno('a', "%s used SETNAME to change their real name to '%s'",
					user->nick.c_str(), parameters[0].c_str());
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

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("setname");

		// Whether the module should only be usable by server operators.
		bool operonly = tag->getBool("operonly");
		cmd.flags_needed = operonly ? 'o' : 0;

		// Whether a snotice should be sent out when a user changes their real name.
		cmd.notifyopers = tag->getBool("notifyopers", !operonly);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the SETNAME command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSetName)
