#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"
#include <string>


/* $ModDesc: Povides support for services +r user/chan modes and more */

Server *Srv;
	 

class ModuleServices : public Module
{
 public:
	ModuleServices()
	{
		Srv = new Server;

		Srv->AddExtendedMode('r',MT_CHANNEL,false,0,0);
		Srv->AddExtendedMode('r',MT_CLIENT,false,0,0);
	}
	
	virtual bool OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		
		if (modechar != 'r') {
			return 0;
		}
		
		if (type == MT_CHANNEL)
		{
			// only a u-lined server may add or remove the +r mode.
			if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)))
			{
				return 1;
			}
			else
			{
				Srv->SendServ(user->fd,"500 "+std::string(user->nick)+" :Only a U-Lined server may modify the +r channel mode");
			}
		}
		else
		{
			if (!strcmp(user->server,""))
			{
				return 1;
			}
			else
			{
				Srv->SendServ(user->fd,"500 "+std::string(user->nick)+" :Only a server may modify the +r user mode");
			}
		}

		return 0;
	}
	
	virtual ~ModuleServices()
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


class ModuleServicesFactory : public ModuleFactory
{
 public:
	ModuleServicesFactory()
	{
	}
	
	~ModuleServicesFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleServices;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleServicesFactory;
}

