#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style channel mode +T */

class ModuleNoNotice : public Module
{
	Server *Srv;
 public:
 
	ModuleNoNotice()
	{
		Srv = new Server;
		Srv->AddExtendedMode('T',MT_CHANNEL,false,0,0);
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string text)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			if (c->IsCustomModeSet('T'))
			{
				if ((Srv->ChanMode(user,c) == "@") || (Srv->ChanMode(user,c) == "%"))
				{
					// ops and halfops can still /NOTICE the channel
					return 0;
				}
				else
				{
					WriteServ(user->fd,"404 %s %s :Can't send NOTICE to channel (+T set)",user->nick, c->name);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'T') && (type == MT_CHANNEL))
  		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	
	virtual ~ModuleNoNotice()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
};


class ModuleNoNoticeFactory : public ModuleFactory
{
 public:
	ModuleNoNoticeFactory()
	{
	}
	
	~ModuleNoNoticeFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleNoNotice;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoNoticeFactory;
}

