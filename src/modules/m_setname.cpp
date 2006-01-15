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

/* $ModDesc: Provides support for the SETNAME command */

Server *Srv;

class cmd_setname : public command_t
{
 public:
	cmd_setname () : command_t("SETNAME", 0, 1)
	{
		this->source = "m_setname.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		std::string line = "";
		for (int i = 0; i < pcnt-1; i++)
		{
			line = line + std::string(parameters[i]);
		}
		line = line + std::string(parameters[pcnt-1]);
		Srv->ChangeGECOS(user,line);
	}
};


class ModuleSetName : public Module
{
	cmd_setname*	mycommand;
 public:
	ModuleSetName(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_setname();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleSetName()
	{
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSetName(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetNameFactory;
}

