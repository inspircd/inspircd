#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style channel mode +Q */

class ModuleNoKicks : public Module
{
	ConfigReader *Conf;
	
 public:
 
	ModuleNoKicks()
	{
		Srv = new Server;
		Srv->AddExtendedMode('Q',MT_CHANNEL,false,0,0);
	}
	
	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type)
	{
		if (access_type == AC_KICK)
		{
			if (channel->IsCustomModeSet('Q'))
			{
				if ((Srv->IsUlined(source->nick)) || (Srv->IsUlined(source->server)) || (!strcmp(source->server,"")))
				{
					// ulines can still kick with +Q in place
					return ACR_ALLOW;
				}
				else
				{
					// nobody else can (not even opers with override, and founders)
					WriteServ(source->fd,"484 %s %s :Can't kick user %s from channel (+Q set)",source->nick, c->name,dest->nick);
					return ACR_DENY;
				}
			}
		}
		return ACR_DEFAULT;
	}
	
	virtual ~ModuleNoKicks()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
};


class ModuleNoKicksFactory : public ModuleFactory
{
 public:
	ModuleNoKicksFactory()
	{
	}
	
	~ModuleNoKicksFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleNoKicks;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoKicksFactory;
}

