#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Povides a proof-of-concept test /WOOT command */

Server *Srv;
	 

void handle_woot(char **parameters, int pcnt, userrec *user)
{
	// this test command just accepts:
	// /woot <text>
	// and sends <text> to all opers with +s mode.
	Srv->SendOpers(parameters[0]);
}

class ModuleTestCommand : public Module
{
 public:
	ModuleTestCommand()
	{
		Srv = new Server;
		Srv->AddCommand("WOOT",handle_woot,0,1)
	}
	
	virtual ~ModuleTestCommand()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
	}

};


class ModuleTestCommandFactory : public ModuleFactory
{
 public:
	ModuleTestCommandFactory()
	{
	}
	
	~ModuleTestCommandFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleTestCommand;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleTestCommandFactory;
}

