#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style GLOBOPS and umode +g */

Server *Srv;

class ModuleNoNickChange : public Module
{
 public:
	ModuleNoNickChange()
	{
		Srv = new Server;
		
		Srv->AddExtendedMode('N',MT_CHANNEL,false,0,0);
	}
	
	virtual ~ModuleNoNickChange()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1);
	}
	
	virtual int OnUserPreNick(userrec* user, std::string newnick)
	{
		if (!strcasecmp(user->server,Srv->GetServerName().c_str()))
		{
			for (int i =0; i != MAXCHANS; i++)
			{
				if (user->chans[i].channel != NULL)
				{
					chanrec* curr = user->chans[i].channel;
					if (curr->IsCustomModeSet('N'))
					{
						if (!strchr(user->modes,'o'))
						{
							// don't allow the nickchange, theyre on at least one channel with +N set
							// and theyre not an oper
							WriteServ(user->fd,"447 %s :Can't change nickname while on %s (+N is set)",user->nick,curr->name);
							return 1;
						}
					}
				}
			}
		}
		return 0;
	}
 	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'N') && (type == MT_CHANNEL))
  		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleNoNickChangeFactory : public ModuleFactory
{
 public:
	ModuleNoNickChangeFactory()
	{
	}
	
	~ModuleNoNickChangeFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleNoNickChange;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoNickChangeFactory;
}

