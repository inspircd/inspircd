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
#include "inspircd.h"

/* $ModDesc: Provides support for unreal-style channel mode +c */



class NoCTCP : public ModeHandler
{
 public:
	NoCTCP(InspIRCd* Instance) : ModeHandler(Instance, 'C', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('C'))
			{
				channel->SetMode('C',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('C'))
			{
				channel->SetMode('C',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoCTCP : public Module
{
	
	NoCTCP* nc;
	
 public:
 
	ModuleNoCTCP(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		nc = new NoCTCP(ServerInstance);
		ServerInstance->AddMode(nc, 'C');
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->Modes->InsertMode(output,"C",4);
	}
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		return OnUserPreNotice(user,dest,target_type,text,status);
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			if (c->IsModeSet('C'))
			{
				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						user->WriteServ("492 %s %s :Can't send CTCP to channel (+C set)",user->nick, c->name);
						return 1;
					}
				}
			}
		}
		return 0;
	}

	virtual ~ModuleNoCTCP()
	{
		DELETE(nc);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleNoCTCPFactory : public ModuleFactory
{
 public:
	ModuleNoCTCPFactory()
	{
	}
	
	~ModuleNoCTCPFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleNoCTCP(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoCTCPFactory;
}

