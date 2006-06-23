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

class ModuleBlockCAPS : public Module
{
	Server *Srv;
public:
	
	ModuleBlockCAPS(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		Srv->AddExtendedMode('P', MT_CHANNEL, false, 0, 0);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnExtendedMode] = 1;
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
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'P') && (type == MT_CHANNEL))
  		{
  			log(DEBUG,"Allowing P change");
			return 1;
		}
		else
		{
			return 0;
		}
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
