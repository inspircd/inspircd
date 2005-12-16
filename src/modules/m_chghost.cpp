/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for the CHGHOST command */

Server *Srv;


class cmd_chghost : public command_t
{
 public:
         cmd_chghost () : command_t("CHGHOST",'o',2)
	 {
		 this->source = "m_chghost.so";
	 }
	 
        void Handle(char **parameters, int pcnt, userrec *user)
	{
		for (unsigned int x = 0; x < strlen(parameters[1]); x++)
		{
			if (((tolower(parameters[1][x]) < 'a') || (tolower(parameters[1][x]) > 'z')) && (parameters[1][x] != '.'))
			{
				if (((parameters[1][x] < '0') || (parameters[1][x]> '9')) && (parameters[1][x] != '-'))
				{
					Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+" :*** Invalid characters in hostname");
					return;
				}
			}
		}	
		userrec* dest = Srv->FindNick(std::string(parameters[0]));
		if (dest)
		{
			Srv->ChangeHost(dest,parameters[1]);
			if (!Srv->IsUlined(user->server))
			{
				// fix by brain - ulines set hosts silently
				Srv->SendOpers(std::string(user->nick)+" used CHGHOST to make the displayed host of "+std::string(dest->nick)+" become "+std::string(parameters[1]));
			}
		}
	}
};


class ModuleChgHost : public Module
{
	cmd_chghost* mycommand;
 public:
	ModuleChgHost(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_chghost();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleChgHost()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,2,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleChgHostFactory : public ModuleFactory
{
 public:
	ModuleChgHostFactory()
	{
	}
	
	~ModuleChgHostFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleChgHost(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChgHostFactory;
}

