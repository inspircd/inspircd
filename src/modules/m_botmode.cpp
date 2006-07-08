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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for unreal-style umode +B */

class BotMode : public ModeHandler
{
 public:
	BotMode() : ModeHandler('B', 0, 0, false, MODETYPE_USER, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!dest->IsModeSet('B'))
			{
				dest->SetMode('B',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (dest->IsModeSet('B'))
			{
				dest->SetMode('B',false);
				return MODEACTION_ALLOW;
			}
		}
	}
};

class ModuleBotMode : public Module
{
	Server *Srv;
	BotMode* bm;
 public:
	ModuleBotMode(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		bm = new BotMode();
		Srv->AddMode(bm, 'B');
	}

	void Implements(char* List)
	{
		List[I_OnWhois] = 1;
	}
	
	virtual ~ModuleBotMode()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}

	virtual void OnWhois(userrec* src, userrec* dst)
	{
		if (dst->IsModeSet('B'))
		{
			Srv->SendTo(NULL,src,"335 "+std::string(src->nick)+" "+std::string(dst->nick)+" :is a \2bot\2 on "+Srv->GetNetworkName());
		}
	}

};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleBotModeFactory : public ModuleFactory
{
 public:
	ModuleBotModeFactory()
	{
	}
	
	~ModuleBotModeFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleBotMode(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBotModeFactory;
}

