/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Provides masking of user hostnames via traditional /VHOST command */

/** Handle /VHOST
 */
class CommandVhost : public Command
{
 public:
	CommandVhost(Module* Creator) : Command(Creator,"VHOST", 2)
	{
		syntax = "<username> <password>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags("vhost");
		for(ConfigIter i = tags.first; i != tags.second; ++i)
		{
			ConfigTag* tag = i->second;
			std::string mask = tag->getString("host");
			std::string username = tag->getString("user");
			std::string pass = tag->getString("pass");
			std::string hash = tag->getString("hash");

			if (parameters[0] == username && !ServerInstance->PassCompare(user, pass, parameters[1], hash))
			{
				if (!mask.empty())
				{
					user->WriteServ("NOTICE "+user->nick+" :Setting your VHost: " + mask);
					user->ChangeDisplayedHost(mask.c_str());
					return CMD_SUCCESS;
				}
			}
		}

		user->WriteServ("NOTICE "+user->nick+" :Invalid username or password.");
		return CMD_FAILURE;
	}
};

class ModuleVHost : public Module
{
 private:
	CommandVhost cmd;

 public:
	ModuleVHost() : cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	virtual ~ModuleVHost()
	{
	}


	virtual Version GetVersion()
	{
		return Version("Provides masking of user hostnames via traditional /VHOST command",VF_VENDOR);
	}

};

MODULE_INIT(ModuleVHost)

