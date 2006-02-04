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

/* $ModDesc: Provides support for unreal-style channel mode +c */

class ModuleNoCTCP : public Module
{
	Server *Srv;
	
 public:
 
	ModuleNoCTCP(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Srv->AddExtendedMode('C',MT_CHANNEL,false,0,0);
	}

	void Implements(char* List)
	{
		List[I_OnExtendedMode] = List[I_On005Numeric] = List[I_OnUserPreMessage] = List[I_OnUserPreNotice] = 1;
	}

        virtual void On005Numeric(std::string &output)
        {
		InsertMode(output,"C",4);
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
			if (c->IsCustomModeSet('C'))
			{
				if ((text.length()) && (text[0] == '\1'))
				{
					if (strncmp(text.c_str(),"\1ACTION ",8))
					{
						WriteServ(user->fd,"492 %s %s :Can't send CTCP to channel (+C set)",user->nick, c->name);
						return 1;
					}
				}
			}
		}
		return 0;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'C') && (type == MT_CHANNEL))
  		{
  			log(DEBUG,"Allowing C change");
			return 1;
		}
		else
		{
			return 0;
		}
	}

	virtual ~ModuleNoCTCP()
	{
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleNoCTCP(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoCTCPFactory;
}

