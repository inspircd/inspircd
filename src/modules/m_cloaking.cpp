#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides masking of user hostnames */

class ModuleCloaking : public Module
{
 private:

	 Server *Srv;
	 
 public:
	ModuleCloaking()
	{
		Srv = new Server;
	}
	
	virtual ~ModuleCloaking()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
	
	virtual void OnUserConnect(userrec* user)
	{
		if (strstr(user->dhost,"."))
		{
			std::string a = strstr(user->dhost,".");
			char ra[64];
			long seed,s2;
			memcpy(&seed,user->dhost,sizeof(long));
			memcpy(&s2,a.c_str(),sizeof(long));
			sprintf(ra,"%.8X",seed*s2*strlen(user->host));
			std::string b = Srv->GetNetworkName() + "-" + ra + a;
			Srv->Log(DEBUG,"cloak: allocated "+b);
			strcpy(user->dhost,b.c_str());
		}
	}

};


class ModuleCloakingFactory : public ModuleFactory
{
 public:
	ModuleCloakingFactory()
	{
	}
	
	~ModuleCloakingFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleCloaking;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCloakingFactory;
}

