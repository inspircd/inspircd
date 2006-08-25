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
#include <sstream>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +c */



class BlockColor : public ModeHandler
{
 public:
	BlockColor(InspIRCd* Instance) : ModeHandler(Instance, 'c', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('c'))
			{
				channel->SetMode('c',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('c'))
			{
				channel->SetMode('c',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleBlockColour : public Module
{
	
	BlockColor *bc;
 public:
 
	ModuleBlockColour(InspIRCd* Me) : Module::Module(Me)
	{
		
		bc = new BlockColor(ServerInstance);
		ServerInstance->AddMode(bc, 'c');
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
			
			if(c->IsModeSet('c'))
			{
				/* Replace a strlcpy(), which ran even when +c wasn't set, with this (no copies at all) -- Om */
				for (std::string::iterator i = text.begin(); i != text.end(); i++)
				{
					switch (*i)
					{
						case 2:
						case 3:
						case 15:
						case 21:
						case 22:
						case 31:
							user->WriteServ("404 %s %s :Can't send colours to channel (+c set)",user->nick, c->name);
							return 1;
						break;
					}
				}
			}
		}
		return 0;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreMessage(user,dest,target_type,text,status);
	}

	virtual ~ModuleBlockColour()
	{
		DELETE(bc);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleBlockColourFactory : public ModuleFactory
{
 public:
	ModuleBlockColourFactory()
	{
	}
	
	~ModuleBlockColourFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleBlockColour(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBlockColourFactory;
}
