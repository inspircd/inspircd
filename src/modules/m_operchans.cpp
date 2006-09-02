/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides support for oper-only chans via the +O channel mode */

class OperChans : public ModeHandler
{
 public:
	/* This is an oper-only mode */
	OperChans(InspIRCd* Instance) : ModeHandler(Instance, 'O', 0, 0, false, MODETYPE_CHANNEL, true) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('O'))
			{
				channel->SetMode('O',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('O'))
			{
				channel->SetMode('O',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleOperChans : public Module
{
	
	OperChans* oc;
 public:
	ModuleOperChans(InspIRCd* Me)
		: Module::Module(Me)
	{
				
		oc = new OperChans(ServerInstance);
		ServerInstance->AddMode(oc, 'O');
	}

	void Implements(char* List)
	{
		List[I_OnUserPreJoin] = 1;
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname)
	{
		if (!*user->oper)
		{
			if (chan)
			{
				if (chan->IsModeSet('O'))
				{
					user->WriteServ("520 %s %s :Only IRC operators may join the channel %s (+O is set)",user->nick, chan->name,chan->name);
					return 1;
				}
			}
		}
		return 0;
	}
	
    	virtual ~ModuleOperChans()
	{
		ServerInstance->Modes->DelMode(oc);
		DELETE(oc);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleOperChans(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperChansFactory;
}

