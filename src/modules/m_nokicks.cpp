/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +Q */

class NoKicks : public ModeHandler
{
 public:
	NoKicks(InspIRCd* Instance) : ModeHandler(Instance, 'Q', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
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
		: Module(Me)
	{
		
		nk = new NoKicks(ServerInstance);
		if (!ServerInstance->AddMode(nk))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnAccessCheck };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	virtual int OnAccessCheck(User* source,User* dest,Channel* channel,int access_type)
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
		delete nk;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};


MODULE_INIT(ModuleNoKicks)
