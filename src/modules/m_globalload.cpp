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

/* $ModDesc: Allows global loading of a module. */

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "helperfuncs.h"

extern InspIRCd *ServerInstance;
	 
class cmd_gloadmodule : public command_t
{
 public:
	cmd_gloadmodule () : command_t("GLOADMODULE", 'o', 1)
	{
		this->source = "m_globalload.so";
		syntax = "<modulename>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (ServerInstance->LoadModule(parameters[0]))
		{
			WriteOpers("*** NEW MODULE '%s' GLOBALLY LOADED BY '%s'",parameters[0],user->nick);
			WriteServ(user->fd,"975 %s %s :Module successfully loaded.",user->nick, parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"974 %s %s :Failed to load module: %s",user->nick, parameters[0],ServerInstance->ModuleError());
		}
	}
};

class cmd_gunloadmodule : public command_t
{
 public:
	cmd_gunloadmodule () : command_t("GUNLOADMODULE", 'o', 1)
	{
		this->source = "m_globalload.so";
		syntax = "<modulename>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (ServerInstance->UnloadModule(parameters[0]))
		{
			WriteOpers("*** MODULE '%s' GLOBALLY UNLOADED BY '%s'",parameters[0],user->nick);
			WriteServ(user->fd,"973 %s %s :Module successfully unloaded.",user->nick, parameters[0]);
		}
		else
		{
			WriteServ(user->fd,"972 %s %s :Failed to unload module: %s",user->nick, parameters[0],ServerInstance->ModuleError());
		}
	}
};

class ModuleGlobalLoad : public Module
{
	cmd_gloadmodule *mycommand;
	cmd_gunloadmodule *mycommand2;
	Server *Srv;
 public:
	ModuleGlobalLoad(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_gloadmodule();
		mycommand2 = new cmd_gunloadmodule();
		Srv->AddCommand(mycommand);
		Srv->AddCommand(mycommand2);
	}
	
	virtual ~ModuleGlobalLoad()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}
};


class ModuleGlobalLoadFactory : public ModuleFactory
{
 public:
	ModuleGlobalLoadFactory()
	{
	}
	
	~ModuleGlobalLoadFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleGlobalLoad(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleGlobalLoadFactory;
}
