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
#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +Q */



class NoKicks : public ModeHandler
{
 public:
	NoKicks(InspIRCd* Instance) : ModeHandler(Instance, 'Q', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('Q'))
			{
				channel->SetMode('Q',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('Q'))
			{
				channel->SetMode('Q',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoKicks : public Module
{
	
	NoKicks* nk;
	
 public:
 
	ModuleNoKicks(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		nk = new NoKicks(ServerInstance);
		ServerInstance->AddMode(nk, 'Q');
	}

	void Implements(char* List)
	{
		List[I_OnAccessCheck] = 1;
	}

	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type)
	{
		if (access_type == AC_KICK)
		{
			if (channel->IsModeSet('Q'))
			{
				if ((ServerInstance->ULine(source->nick)) || (ServerInstance->ULine(source->server)) || (!*source->server))
				{
					// ulines can still kick with +Q in place
					return ACR_ALLOW;
				}
				else
				{
					// nobody else can (not even opers with override, and founders)
					source->WriteServ("484 %s %s :Can't kick user %s from channel (+Q set)",source->nick, channel->name,dest->nick);
					return ACR_DENY;
				}
			}
		}
		return ACR_DEFAULT;
	}

	virtual ~ModuleNoKicks()
	{
		ServerInstance->Modes->DelMode(nk);
		DELETE(nk);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleNoKicks(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoKicksFactory;
}

