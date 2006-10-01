/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *                <omster@gmail.com>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"
#include "mode.h"

/* $ModDesc: Provides support for channel mode +P to block all-CAPS channel messages and notices */


/** Handles the +P channel mode
 */
class BlockCaps : public ModeHandler
{
 public:
	BlockCaps(InspIRCd* Instance) : ModeHandler(Instance, 'P', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('P'))
			{
				channel->SetMode('P',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('P'))
			{
				channel->SetMode('P',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleBlockCAPS : public Module
{
	
	BlockCaps* bc;
public:
	
	ModuleBlockCAPS(InspIRCd* Me) : Module::Module(Me)
	{
		
		bc = new BlockCaps(ServerInstance);
		ServerInstance->AddMode(bc, 'P');
	}

	void Implements(char* List)
	{
		List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;

			if (c->IsModeSet('P'))
			{
				const char* i = text.c_str();
				for (; *i; i++)
				{
					if (((*i != ' ') && (*i != '\t')) && ((*i < 'A') || (*i > 'Z')))
					{
						return 0;
					}
				}
				
				user->WriteServ( "404 %s %s :Can't send all-CAPS to channel (+P set)", user->nick, c->name);
				return 1;
			}
		}
		return 0;
	}

	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreMessage(user,dest,target_type,text,status);
	}

	virtual ~ModuleBlockCAPS()
	{
		ServerInstance->Modes->DelMode(bc);
		DELETE(bc);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_COMMON|VF_VENDOR,API_VERSION);
	}
};


class ModuleBlockCAPSFactory : public ModuleFactory
{
 public:
	ModuleBlockCAPSFactory()
	{
	}
	
	~ModuleBlockCAPSFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleBlockCAPS(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBlockCAPSFactory;
}
