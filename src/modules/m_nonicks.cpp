/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

using namespace std;

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"
#include "hashcomp.h"

/* $ModDesc: Provides support for unreal-style GLOBOPS and umode +g */

class NoNicks : public ModeHandler
{
 public:
	NoNicks() : ModeHandler('N', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('N'))
			{
				channel->SetMode('N',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('N'))
			{
				channel->SetMode('N',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleNoNickChange : public Module
{
	Server *Srv;
	NoNicks* nn;
	
 public:
	ModuleNoNickChange(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		nn = new NoNicks();
		Srv->AddMode(nn, 'N');
	}
	
	virtual ~ModuleNoNickChange()
	{
		DELETE(nn);
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_STATIC|VF_VENDOR);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnUserPreNick] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output,"N",4);
	}
	
	virtual int OnUserPreNick(userrec* user, const std::string &newnick)
	{
		irc::string server = user->server;
		irc::string me = Srv->GetServerName().c_str();
		if (server == me)
		{
			for (std::vector<ucrec*>::iterator i = user->chans.begin(); i != user->chans.end(); i++)
			{
				if (((ucrec*)(*i))->channel != NULL)
				{
					chanrec* curr = ((ucrec*)(*i))->channel;
					if ((curr->IsModeSet('N')) && (!*user->oper))
					{
						// don't allow the nickchange, theyre on at least one channel with +N set
						// and theyre not an oper
						WriteServ(user->fd,"447 %s :Can't change nickname while on %s (+N is set)",user->nick,curr->name);
						return 1;
					}
				}
			}
		}
		return 0;
	}
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleNoNickChangeFactory : public ModuleFactory
{
 public:
	ModuleNoNickChangeFactory()
	{
	}
	
	~ModuleNoNickChangeFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleNoNickChange(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleNoNickChangeFactory;
}

