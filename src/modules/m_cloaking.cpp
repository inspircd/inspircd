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
		
		// doesn't require oper, client umode with no params
		// (actually, you cant have params for a umode!)
		if (!Srv->AddExtendedMode('x',MT_CLIENT,false,0,0))
		{
			Srv->Log(DEFAULT,"*** m_cloaking: ERROR, failed to allocate user mode +x!");
			printf("Could not claim usermode +x for this module!");
			exit(0);
		}
	}
	
	virtual ~ModuleCloaking()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1);
	}
	
	virtual bool OnExtendedMode(userrec* user, chanrec* chan, char modechar, int type, bool mode_on, string_list &params)
	{
		Srv->Log(DEBUG,"in mode handler");
		if ((modechar == 'x') && (type == MT_CLIENT))
  		{
			Srv->Log(DEBUG,"this is my mode!");
			if (mode_on)
			{
				Srv->Log(DEBUG,"mode being turned on");
				if (strstr(user->host,"."))
				{
					std::string a = strstr(user->host,".");
					char ra[64];
					long seed,s2;
					memcpy(&seed,user->host,sizeof(long));
					memcpy(&s2,a.c_str(),sizeof(long));
					sprintf(ra,"%.8X",seed*s2*strlen(user->host));
					std::string b = Srv->GetNetworkName() + "-" + ra + a;
					Srv->Log(DEBUG,"cloak: allocated "+b);
					strcpy(user->dhost,b.c_str());
				}
			}
			else
  			{
				Srv->Log(DEBUG,"cloak: de-allocated cloak");
  				strcpy(user->dhost,user->host);
			}
			return 1;
		}
		else
		{
			// this mode isn't ours, we have to bail and return 0 to not handle it.
			Srv->Log(DEBUG,"not my mode");
			return 0;
		}
	}

	virtual void OnUserConnect(userrec* user)
	{
		Srv->Log(DEBUG,"Sending SAMODE +x for user");
		char* modes[2];
		modes[0] = user->nick;
		modes[1] = "+x";
		Srv->SendMode(modes,2,user);
		Srv->Log(DEBUG,"Sent SAMODE +x for user");
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

