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

/* $ModDesc: Provides support for the SETNAME command */



class cmd_setname : public command_t
{
 public:
 cmd_setname (InspIRCd* Instance) : command_t(Instance,"SETNAME", 0, 1)
	{
		this->source = "m_setname.so";
		syntax = "<new-gecos>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		std::string line = "";
		for (int i = 0; i < pcnt-1; i++)
		{
			line = line + std::string(parameters[i]) + " ";
		}
		line = line + std::string(parameters[pcnt-1]);
		user->ChangeName(line.c_str());
	}
};


class ModuleSetName : public Module
{
	cmd_setname*	mycommand;
 public:
	ModuleSetName(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_setname(ServerInstance);
		ServerInstance->AddCommand(mycommand);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSetName(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetNameFactory;
}

