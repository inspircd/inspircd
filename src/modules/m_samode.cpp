i/*
 * SAMODE module for InspIRCd
 *  Co authored by Brain and w00t
 *
 *  Syntax: /SAMODE <#chan/nick> +/-<modes> [parameters for modes]
 * 
 */

/* $ModDesc: Povides more advanced UnrealIRCd SAMODE command */

/*
 * ToDo:
 *   Err... not a lot really.
 */ 

#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"

Server *Srv;
	 

void handle_samode(char **parameters, int pcnt, userrec *user)
{
	/*
	 * Handles an SAMODE request. Notifies all +s users.
 	 */
	int n=0;
	std::string result;
	Srv->Log(DEBUG,"SAMODE: Being handled");
	Srv->SendMode(parameters,pcnt,user);
	Srv->Log(DEBUG,"SAMODE: Modechange handled");
	result = std::string(user->nick) + std::string(" used SAMODE ");
  	while (n<pcnt)
	{
		result=result + std::string(" ") + std::string(parameters[n]);
		n++;
	}
      Srv->SendOpers(result);
}

class ModuleSaMode : public Module
{
 public:
	ModuleSaMode()
	{
		Srv = new Server;
                Srv->Log(DEBUG,"SAMODE: Pre-add cmd");
		Srv->AddCommand("SAMODE",handle_samode,'o',2);
		Srv->Log(DEBUG,"SAMODE: Post-add cmd");
	}
	
	virtual ~ModuleSaMode()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,2,1);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
	}

};


class ModuleSaModeFactory : public ModuleFactory
{
 public:
	ModuleSaModeFactory()
	{
	}
	
	~ModuleSaModeFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleSaMode;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSaModeFactory;
}
