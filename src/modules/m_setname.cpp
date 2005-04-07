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

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for the SETNAME command */

Server *Srv;
	 
void handle_setname(char **parameters, int pcnt, userrec *user)
{
	std::string line = "";
	for (int i = 0; i < pcnt-1; i++)
	{
		line = line + std::string(parameters[i]);
	}
	line = line + std::string(parameters[pcnt-1]);
	Srv->ChangeGECOS(user,line);
}


class ModuleSetName : public Module
{
 public:
	ModuleSetName()
	{
		Srv = new Server;
		Srv->AddCommand("SETNAME",handle_setname,0,1,"m_setname.so");
	}
	
	virtual ~ModuleSetName()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSetNameFactory : public ModuleFactory
{
 public:
	ModuleSetNameFactory()
	{
	}
	
	~ModuleSetNameFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleSetName;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetNameFactory;
}

