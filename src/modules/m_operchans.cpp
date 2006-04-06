/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for oper-only chans via the +O channel mode */

class ModuleOperChans : public Module
{
	Server* Srv;
 public:
	ModuleOperChans(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		// Add a mode +O for channels with no parameters		
		Srv->AddExtendedMode('O',MT_CHANNEL,false,0,0);
	}

	void Implements(char* List)
	{
		List[I_OnExtendedMode] = List[I_On005Numeric] = List[I_OnUserPreJoin] = 1;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		if ((modechar == 'O') && (type == MT_CHANNEL))
		{
			chanrec* chan = (chanrec*)target;
			
			if ((Srv->IsUlined(user->nick)) || (Srv->IsUlined(user->server)) || (!*user->server) || (*user->oper))
			{
				log(DEBUG,"Allowing mode +O");
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

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output,"O",4);
	}
	
	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (!*user->oper)
		{
			if (chan)
			{
				if (chan->IsModeSet('O'))
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
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleOperChans(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperChansFactory;
}

