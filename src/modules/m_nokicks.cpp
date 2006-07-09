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

/* $ModDesc: Provides support for unreal-style channel mode +Q */

class ModuleNoKicks : public Module
{
	Server *Srv;
	
 public:
 
	ModuleNoKicks(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Srv->AddExtendedMode('Q',MT_CHANNEL,false,0,0);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnAccessCheck] = List[I_OnExtendedMode] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output,"Q",4);
	}

	virtual int OnAccessCheck(userrec* source,userrec* dest,chanrec* channel,int access_type)
	{
		if (access_type == AC_KICK)
		{
			if (channel->IsModeSet('Q'))
			{
				if ((Srv->IsUlined(source->nick)) || (Srv->IsUlined(source->server)) || (!strcmp(source->server,"")))
				{
					// ulines can still kick with +Q in place
					return ACR_ALLOW;
				}
				else
				{
					// nobody else can (not even opers with override, and founders)
					WriteServ(source->fd,"484 %s %s :Can't kick user %s from channel (+Q set)",source->nick, channel->name,dest->nick);
					return ACR_DENY;
				}
			}
		}
		return ACR_DEFAULT;
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'Q') && (type == MT_CHANNEL))
  		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

	virtual ~ModuleNoKicks()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleNoKicksFactory : public ModuleFactory
{
 public:
	ModuleNoKicksFactory()
	{
	}
	
	~ModuleNoKicksFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleNoKicks(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoKicksFactory;
}

