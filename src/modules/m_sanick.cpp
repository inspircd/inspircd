// Sanick and +g support module by C.J.Edwards

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for SANICK command */

Server *Srv;
	 
void handle_sanick(char **parameters, int pcnt, userrec *user)
{
	userrec* source = Srv->FindNick(std::string(parameters[0]));
	if (source)
	{
		if (Srv->IsNick(std::string(parameters[1])))
		{
			Srv->SendOpers(std::string(user->nick)+" used SANICK to change "+std::string(source->nick)+" to "+parameters[1]);
			Srv->ChangeUserNick(source,std::string(parameters[1]));
		}
	}
}


class ModuleSanick : public Module
{
 public:
	ModuleSanick()
	{
		Srv = new Server;
		Srv->AddCommand("SANICK",handle_sanick,'o',2);
	}
	
	virtual ~ModuleSanick()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSanickFactory : public ModuleFactory
{
 public:
	ModuleSanickFactory()
	{
	}
	
	~ModuleSanickFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleSanick;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSanickFactory;
}

