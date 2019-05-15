/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2006 Craig Edwards <craigedwards@brainbox.cc>
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

/** Handle /SETHOST
 */
class CommandSethost : public Command
{
 public:
	std::bitset<UCHAR_MAX> hostmap;

	CommandSethost(Module* Creator)
		: Command(Creator,"SETHOST", 1)
	{
		allow_empty_last_param = false;
		flags_needed = 'o'; syntax = "<host>";
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (parameters[0].length() > ServerInstance->Config->Limits.MaxHost)
		{
			user->WriteNotice("*** SETHOST: Host too long");
			return CMD_FAILURE;
		}

		for (std::string::const_iterator x = parameters[0].begin(); x != parameters[0].end(); x++)
		{
			if (!hostmap.test(static_cast<unsigned char>(*x)))
			{
				user->WriteNotice("*** SETHOST: Invalid characters in hostname");
				return CMD_FAILURE;
			}
		}

		if (user->ChangeDisplayedHost(parameters[0]))
		{
			ServerInstance->SNO.WriteGlobalSno('a', user->nick+" used SETHOST to change their displayed host to "+user->GetDisplayedHost());
			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
	}
};


class ModuleSetHost : public Module
{
	CommandSethost cmd;

 public:
	ModuleSetHost()
		: cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		std::string hmap = ServerInstance->Config->ConfValue("hostname")->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789");

		cmd.hostmap.reset();
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			cmd.hostmap.set(static_cast<unsigned char>(*n));
	}

	Version GetVersion() override
	{
		return Version("Provides the SETHOST command", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSetHost)
