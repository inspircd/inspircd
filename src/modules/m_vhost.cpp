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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides masking of user hostnames via traditional /VHOST command */

static ConfigReader* Conf;

/** Handle /VHOST
 */
class cmd_vhost : public command_t
{
 public:
	cmd_vhost (InspIRCd* Instance) : command_t(Instance,"VHOST", 0, 2)
	{
		this->source = "m_vhost.so";
		syntax = "<username> <password>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
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
					return CMD_FAILURE;
				}
			}
		}
		user->WriteServ("NOTICE "+std::string(user->nick)+" :Invalid username or password.");
		return CMD_FAILURE;
	}
};

class ModuleVHost : public Module
{
 private:

	cmd_vhost* mycommand;
	 
 public:
	ModuleVHost(InspIRCd* Me) : Module(Me)
	{
		
		Conf = new ConfigReader(ServerInstance);
		mycommand = new cmd_vhost(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleVHost()
	{
		DELETE(Conf);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = 1;
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		DELETE(Conf);
		Conf = new ConfigReader(ServerInstance);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

MODULE_INIT(ModuleVHost)

