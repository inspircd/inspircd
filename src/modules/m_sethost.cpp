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

#include "inspircd.h"

/* $ModDesc: Provides support for the SETHOST command */

/** Handle /SETHOST
 */
class cmd_sethost : public command_t
{
 public:
	cmd_sethost (InspIRCd* Instance) : command_t(Instance,"SETHOST",'o',1)
	{
		this->source = "m_sethost.so";
		syntax = "<new-hostname>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		size_t len = 0;
		for (const char* x = parameters[0]; *x; x++, len++)
		{
			if (((tolower(*x) < 'a') || (tolower(*x) > 'z')) && (*x != '.'))
			{
				if (((*x < '0') || (*x> '9')) && (*x != '-'))
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in hostname");
					return CMD_FAILURE;
				}
			}
		}
		if (len > 64)
		{
			user->WriteServ("NOTICE %s :*** SETHOST: Host too long",user->nick);
			return CMD_FAILURE;
		}
		if (user->ChangeDisplayedHost(parameters[0]))
		{
			ServerInstance->WriteOpers(std::string(user->nick)+" used SETHOST to change their displayed host to "+user->dhost);
			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
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
		return Version(1,0,0,1,VF_VENDOR,API_VERSION);
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

