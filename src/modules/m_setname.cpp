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

/* $ModDesc: Provides support for the SETNAME command */



class cmd_setname : public command_t
{
 public:
	cmd_setname (InspIRCd* Instance) : command_t(Instance,"SETNAME", 0, 1)
	{
		this->source = "m_setname.so";
		syntax = "<new-gecos>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		std::string line;
		for (int i = 0; i < pcnt-1; i++)
		{
			line = line + std::string(parameters[i]) + " ";
		}
		line = line + std::string(parameters[pcnt-1]);
		user->ChangeName(line.c_str());

		return CMD_SUCCESS;
	}
};


class ModuleSetName : public Module
{
	cmd_setname*	mycommand;
 public:
	ModuleSetName(InspIRCd* Me)
		: Module(Me)
	{
		
		mycommand = new cmd_setname(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSetName()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
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


extern "C" DllExport void * init_module( void )
{
	return new ModuleSetNameFactory;
}

