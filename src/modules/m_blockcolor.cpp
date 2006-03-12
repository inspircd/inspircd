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
#include "helperfuncs.h"

/* $ModDesc: Provides support for unreal-style channel mode +c */

class ModuleBlockColour : public Module
{
	Server *Srv;
 public:
 
	ModuleBlockColour(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		Srv->AddExtendedMode('c',MT_CHANNEL,false,0,0);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = List[I_OnExtendedMode] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output,"c",4);
	}
	
	virtual int OnUserPreMessage(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			
			if(c->IsModeSet('c'))
			{
				/* Replace a strlcpy(), which ran even when +c wasn't set, with this (no copies at all) -- Om */
				for(unsigned int i = 0; i < text.length(); i++)
				{
					switch (text[i])
					{
						case 2:
						case 3:
						case 15:
						case 21:
						case 22:
						case 31:
							WriteServ(user->fd,"404 %s %s :Can't send colours to channel (+c set)",user->nick, c->name);
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
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'c') && (type == MT_CHANNEL))
  		{
  			log(DEBUG,"Allowing c change");
			return 1;
		}
		else
		{
			return 0;
		}
	}

	virtual ~ModuleBlockColour()
	{
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleBlockColour(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBlockColourFactory;
}
