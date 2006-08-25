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

/* $ModDesc: Provides support for unreal-style channel mode +T */

class NoNotice : public ModeHandler
{
 public:
	NoNotice(InspIRCd* Instance) : ModeHandler(Instance, 'T', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('T'))
			{
				channel->SetMode('T',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('T'))
			{
				channel->SetMode('T',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoNotice : public Module
{
	
	NoNotice* nt;
 public:
 
	ModuleNoNotice(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		nt = new NoNotice(ServerInstance);
		ServerInstance->AddMode(nt, 'T');
	}

	void Implements(char* List)
	{
		List[I_OnUserPreNotice] = 1;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			if (c->IsModeSet('T'))
			{
				if ((ServerInstance->ULine(user->server)) || (c->GetStatus(user) == STATUS_OP) || (c->GetStatus(user) == STATUS_HOP))
				{
					// ops and halfops can still /NOTICE the channel
					return 0;
				}
				else
				{
					user->WriteServ("404 %s %s :Can't send NOTICE to channel (+T set)",user->nick, c->name);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual ~ModuleNoNotice()
	{
		DELETE(nt);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleNoNotice(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoNoticeFactory;
}

