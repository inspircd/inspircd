#include <stdio.h>

#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for oper-only chans via the +O channel mode */

Server *Srv;
	 

class ModuleOperChans : public Module
{
 public:
	ModuleOperChans()
	{
		Srv = new Server;

		// Add a mode +O for channels with no parameters		
		Srv->AddExtendedMode('O',MT_CHANNEL,false,0,0);
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		if ((modechar == 'O') && (type == MT_CHANNEL))
		{
			chanrec* chan = (chanrec*)target;
			
			if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!strcmp(user->server,"")) || (strchr(user->modes,'o')))
			{
				return 1;
			}
			else
			{
				// eat the mode change, return an error
				WriteServ(user->fd,"468 %s %s :Only servers and opers may set channel mode +O",user->nick, chan->name);
				return 0;
			}
	
			// must return 1 to handle the mode!
			return 1;
		}
		
		return 0;
	}
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (!strchr(user->modes,'o'))
		{
			if (chan)
			{
				if (chan->IsCustomModeSet('O'))
				{
					WriteServ(user->fd,"520 %s %s :Only IRC operators may join the channel %s (+O is set)",user->nick, chan->name,chan->name);
					return 1;
				}
			}
		}
		return 0;
	}
	
    	virtual ~ModuleOperChans()
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


class ModuleOperChansFactory : public ModuleFactory
{
 public:
	ModuleOperChansFactory()
	{
	}
	
	~ModuleOperChansFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleOperChans;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperChansFactory;
}

