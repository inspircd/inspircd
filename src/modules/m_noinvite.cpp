/*   +------------------------------------+
 *   | Inspire Internet Relay Chat Daemon |
 *   +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		   E-mail:
 *	    <brain@chatspike.net>
 *	    <Craig@chatspike.net>
 * 
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 * the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for unreal-style channel mode +V */

class ModuleNoInvite : public Module
{
	Server *Srv;
	
	public:
 
		ModuleNoInvite(Server* Me)
			: Module::Module(Me)
		{
			Srv = Me;
			Srv->AddExtendedMode('V',MT_CHANNEL,false,0,0);
		}

		void Implements(char* List)
		{
			List[I_On005Numeric] = List[I_OnUserPreInvite] = List[I_OnExtendedMode] = 1;
		}

		virtual void On005Numeric(std::string &output)
		{
			InsertMode(output,"V",4);
		}


		virtual int OnUserPreInvite(userrec* user,userrec* dest,chanrec* channel)
		{
			if (channel->IsModeSet('V'))
			{
				WriteServ(user->fd,"492 %s %s :Can't invite %s to channel (+V set)",user->nick, channel->name, dest->nick);
				return 1;
			}
			return 0;
		}
	
		virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
		{
			// check if this is our mode character...
			if ((modechar == 'V') && (type == MT_CHANNEL))
  			{
				return 1;
			}
			else
			{
				return 0;
			}
		}

		virtual ~ModuleNoInvite()
		{
		}
	
		virtual Version GetVersion()
		{
			return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
		}
};


class ModuleNoInviteFactory : public ModuleFactory
{
	public:
		ModuleNoInviteFactory()
		{
		}
	
		~ModuleNoInviteFactory()
		{
		}
	
		virtual Module * CreateModule(Server* Me)
		{
			return new ModuleNoInvite(Me);
		}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoInviteFactory;
}

