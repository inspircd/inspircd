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
#include "helperfuncs.h"

/* $ModDesc: Provides support for channel mode +P to block all-CAPS channel messages and notices */

class BlockCaps : public ModeHandler
{
 public:
	BlockCaps() : ModeHandler('P', 0, 0, false, MODETYPE_CHANNEL, false) { }

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
	Server *Srv;
	BlockCaps* bc;
public:
	
	ModuleBlockCAPS(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		bc = new BlockCaps;
		Srv->AddMode(bc, 'P');
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output, "P", 4);
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
				
				WriteServ(user->fd, "404 %s %s :Can't send all-CAPS to channel (+P set)", user->nick, c->name);
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
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleBlockCAPS(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBlockCAPSFactory;
}
