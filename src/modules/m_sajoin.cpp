// Sajoin and +g support module by C.J.Edwards

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style GLOBOPS and umode +g */

Server *Srv;
	 
void handle_sajoin(char **parameters, int pcnt, userrec *user)
{
	userrec* dest = Srv->FindNick(std::string(parameters[0]));
	if (dest)
	{
		Srv->SendOpers(std::string(user->nick)+" used SAJOIN to make "+std::String(dest->nick)+" join "+parameters[1]);
		Srv->JoinUserToChannel(dest,std::String(parameters[1]),std::string(dest->nick));
	}
}


class ModuleSajoin : public Module
{
 public:
	ModuleSajoin()
	{
		Srv = new Server;
		Srv->AddCommand("SAJOIN",handle_sajoin,'o',2);
	}
	
	virtual ~ModuleSajoin()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSajoinFactory : public ModuleFactory
{
 public:
	ModuleSajoinFactory()
	{
	}
	
	~ModuleSajoinFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleSajoin;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSajoinFactory;
}

