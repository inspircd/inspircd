/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for unreal-style channel mode +T */

class ModuleNoNotice : public Module
{
	Server *Srv;
 public:
 
	ModuleNoNotice(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Srv->AddExtendedMode('T',MT_CHANNEL,false,0,0);
	}

	void Implements(char* List)
	{
		List[I_OnUserPreNotice] = List[I_On005Numeric] = List[I_OnExtendedMode] = 1;
	}
	
	virtual int OnUserPreNotice(userrec* user,void* dest,int target_type, std::string &text, char status)
	{
		if (target_type == TYPE_CHANNEL)
		{
			chanrec* c = (chanrec*)dest;
			if (c->IsModeSet('T'))
			{
				if ((Srv->IsUlined(user->server)) || (Srv->ChanMode(user,c) == "@") || (Srv->ChanMode(user,c) == "%"))
				{
					// ops and halfops can still /NOTICE the channel
					return 0;
				}
				else
				{
					WriteServ(user->fd,"404 %s %s :Can't send NOTICE to channel (+T set)",user->nick, c->name);
					return 1;
				}
			}
		}
		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output,"T",4);
	}

	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'T') && (type == MT_CHANNEL))
  		{
			return 1;
		}
		else
		{
			return 0;
		}
	}
	
	virtual ~ModuleNoNotice()
	{
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleNoNotice(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoNoticeFactory;
}

