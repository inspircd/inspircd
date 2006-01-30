/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides masking of user hostnames via traditional /VHOST command */

static ConfigReader *Conf;
static Server* Srv;

class cmd_vhost : public command_t
{
 public:
        cmd_vhost() : command_t("VHOST", 0, 2)
        {
                this->source = "m_vhost.so";
        }

        void Handle (char **parameters, int pcnt, userrec *user)
        {
                for (int index = 0; index < Conf->Enumerate("vhost"); index++)
                {
                        std::string mask = Conf->ReadValue("vhost","host",index);
			std::string username = Conf->ReadValue("vhost","user",index);
			std::string pass = Conf->ReadValue("vhost","pass",index);
                        if ((!strcmp(parameters[0],username.c_str())) && (!strcmp(parameters[1],pass.c_str())))
                        {
                                if (mask != "")
                                {
                                        Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :Setting your VHost: " + mask);
                                        Srv->ChangeHost(user,mask);
                                        return;
                                }
                        }
                }
		Srv->SendServ(user->fd,"NOTICE "+std::string(user->nick)+" :Invalid username or password.");
        }
};

class ModuleVHost : public Module
{
 private:

	cmd_vhost* mycommand;
	 
 public:
	ModuleVHost(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
                Conf = new ConfigReader;
                mycommand = new cmd_vhost();
                Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleVHost()
	{
		delete Conf;
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = 1;
	}

	virtual void OnRehash(std::string parameter)
	{
		delete Conf;
		Conf = new ConfigReader;
	}
	
	virtual Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleVHostFactory : public ModuleFactory
{
 public:
	ModuleVHostFactory()
	{
	}
	
	~ModuleVHostFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleVHost(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleVHostFactory;
}

