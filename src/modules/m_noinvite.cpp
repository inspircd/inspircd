/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                   E-mail:
 *            <brain@chatspike.net>
 *            <Craig@chatspike.net>
 * 
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 * the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +V */

extern InspIRCd* ServerInstance;

class NoInvite : public ModeHandler
{
 public:
	NoInvite(InspIRCd* Instance) : ModeHandler(Instance, 'V', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('V'))
			{
				channel->SetMode('V',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('V'))
			{
				channel->SetMode('V',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoInvite : public Module
{
	Server *Srv;
	NoInvite *ni;
	
	public:
 
		ModuleNoInvite(InspIRCd* Me) : Module::Module(Me)
		{
			
			ni = new NoInvite(ServerInstance);
			Srv->AddMode(ni, 'V');
		}

		void Implements(char* List)
		{
			List[I_On005Numeric] = List[I_OnUserPreInvite] = 1;
		}

		virtual void On005Numeric(std::string &output)
		{
			ServerInstance->ModeGrok->InsertMode(output,"V",4);
		}


		virtual int OnUserPreInvite(userrec* user,userrec* dest,chanrec* channel)
		{
			if (channel->IsModeSet('V'))
			{
				user->WriteServ("492 %s %s :Can't invite %s to channel (+V set)",user->nick, channel->name, dest->nick);
				return 1;
			}
			return 0;
		}

		virtual ~ModuleNoInvite()
		{
			DELETE(ni);
		}
	
		virtual Version GetVersion()
		{
			return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
		}
};


class ModuleNoInviteFactory : public ModuleFactory
{
	public:
		ModuleNoInviteFactory()
		{
		}
	
		~ModuleNoInviteFactory()
		{
		}
	
		virtual Module * CreateModule(InspIRCd* Me)
		{
			return new ModuleNoInvite(Me);
		}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoInviteFactory;
}

