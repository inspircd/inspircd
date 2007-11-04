/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
	CommandVhost (InspIRCd* Instance) : Command(Instance,"VHOST", 0, 2)
	{
		this->source = "m_vhost.so";
		syntax = "<username> <password>";
	}

	CmdResult Handle (const char** parameters, int pcnt, User *user)
	{
		ConfigReader *Conf = new ConfigReader(ServerInstance);

		for (int index = 0; index < Conf->Enumerate("vhost"); index++)
		{
			std::string mask = Conf->ReadValue("vhost","host",index);
			std::string username = Conf->ReadValue("vhost","user",index);
			std::string pass = Conf->ReadValue("vhost","pass",index);

			if ((!strcmp(parameters[0],username.c_str())) && (!strcmp(parameters[1],pass.c_str())))
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
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleVHost)

