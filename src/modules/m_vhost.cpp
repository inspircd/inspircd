/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
	CommandVhost (InspIRCd* Instance) : Command(Instance,"VHOST", 0, 2)
	{
		this->source = "m_vhost.so";
		syntax = "<username> <password>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		ConfigReader *Conf = new ConfigReader(ServerInstance);

		for (int index = 0; index < Conf->Enumerate("vhost"); index++)
		{
			std::string mask = Conf->ReadValue("vhost","host",index);
			std::string username = Conf->ReadValue("vhost","user",index);
			std::string pass = Conf->ReadValue("vhost","pass",index);
			std::string hash = Conf->ReadValue("vhost","hash",index);

			if ((!strcmp(parameters[0].c_str(),username.c_str())) && !ServerInstance->PassCompare(user, pass.c_str(), parameters[1].c_str(), hash.c_str()))
			{
				if (!mask.empty())
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :Setting your VHost: " + mask);
					user->ChangeDisplayedHost(mask.c_str());
					delete Conf;
					return CMD_LOCALONLY;
				}
			}
		}

		user->WriteServ("NOTICE "+std::string(user->nick)+" :Invalid username or password.");
		delete Conf;
		return CMD_FAILURE;
	}
};

class ModuleVHost : public Module
{
 private:

	CommandVhost* mycommand;

 public:
	ModuleVHost(InspIRCd* Me) : Module(Me)
	{
		mycommand = new CommandVhost(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleVHost()
	{
	}


	virtual Version GetVersion()
	{
		return Version("$Id$",VF_VENDOR,API_VERSION);
	}

};

MODULE_INIT(ModuleVHost)

