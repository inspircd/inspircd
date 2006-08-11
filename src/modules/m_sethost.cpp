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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Provides support for the SETHOST command */




class cmd_sethost : public command_t
{
 public:
 cmd_sethost (InspIRCd* Instance) : command_t(Instance,"SETHOST",'o',1)
	{
		this->source = "m_sethost.so";
		syntax = "<new-hostname>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (strlen(parameters[0]) > 64)
		{
			user->WriteServ("NOTICE %s :*** SETHOST: Host too long",user->nick);
			return;
		}
		for (unsigned int x = 0; x < strlen(parameters[0]); x++)
		{
			if (((tolower(parameters[0][x]) < 'a') || (tolower(parameters[0][x]) > 'z')) && (parameters[0][x] != '.'))
			{
				if (((parameters[0][x] < '0') || (parameters[0][x]> '9')) && (parameters[0][x] != '-'))
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in hostname");
					return;
				}
			}
		}
		if (user->ChangeDisplayedHost(parameters[0]))
			ServerInstance->WriteOpers(std::string(user->nick)+" used SETHOST to change their displayed host to "+std::string(parameters[0]));
	}
};


class ModuleSetHost : public Module
{
	cmd_sethost*	mycommand;
 public:
	ModuleSetHost(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_sethost(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSetHost()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSetHostFactory : public ModuleFactory
{
 public:
	ModuleSetHostFactory()
	{
	}
	
	~ModuleSetHostFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSetHost(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetHostFactory;
}

