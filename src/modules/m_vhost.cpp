/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides masking of user hostnames via traditional /VHOST command */

/** Handle /VHOST
 */
class CommandVhost : public Command
{
 public:
	CommandVhost (InspIRCd* Instance, Module* Creator) : Command(Instance, Creator,"VHOST", 0, 2)
	{
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
	CommandVhost cmd;

 public:
	ModuleVHost(InspIRCd* Me) : Module(Me), cmd(Me, this)
	{
		ServerInstance->AddCommand(&cmd);
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

