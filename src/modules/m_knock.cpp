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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for /KNOCK and mode +K */

static Server *Srv;

class cmd_knock : public command_t
{
 public:
	cmd_knock () : command_t("KNOCK", 0, 2)
	{
		this->source = "m_knock.so";
	}
	
	void Handle (char **parameters, int pcnt, userrec *user)
	{
		chanrec* c = Srv->FindChannel(parameters[0]);

		if (!c)
		{
			WriteServ(user->fd,"401 %s %s :No such channel",user->nick, parameters[0]);
			return;
		}

		std::string line = "";

		if (c->IsModeSet('K'))
		{
			WriteServ(user->fd,"480 %s :Can't KNOCK on %s, +K is set.",user->nick, c->name);
			return;
		}

		for (int i = 1; i < pcnt - 1; i++)
		{
			line = line + std::string(parameters[i]) + " ";
		}
		line = line + std::string(parameters[pcnt-1]);

		if (c->modes[CM_INVITEONLY])
		{
			WriteChannelWithServ((char*)Srv->GetServerName().c_str(),c,"NOTICE %s :User %s is KNOCKing on %s (%s)",c->name,user->nick,c->name,line.c_str());
			WriteServ(user->fd,"NOTICE %s :KNOCKing on %s",user->nick,c->name);
			return;
		}
		else
		{
			WriteServ(user->fd,"480 %s :Can't KNOCK on %s, channel is not invite only so knocking is pointless!",user->nick, c->name);
			return;
		}
	}
};

class Knock : public ModeHandler
{
 public:
	Knock() : ModeHandler('K', 0, 0, false, MODETYPE_CHANNEL, false) { }

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			if (!channel->IsModeSet('K'))
			{
				channel->SetMode('K',true);
				return MODEACTION_ALLOW;
			}
		}
		else
		{
			if (channel->IsModeSet('K'))
			{
				channel->SetMode('K',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}
};

class ModuleKnock : public Module
{
	cmd_knock* mycommand;
	Knock* kn;
 public:
	ModuleKnock(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		kn = new Knock();
		Srv->AddMode(kn, 'K');
		mycommand = new cmd_knock();
		Srv->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		InsertMode(output,"K",4);
	}

	virtual ~ModuleKnock()
	{
		DELETE(kn);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_STATIC|VF_VENDOR);
	}
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleKnockFactory : public ModuleFactory
{
 public:
	ModuleKnockFactory()
	{
	}
	
	~ModuleKnockFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleKnock(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleKnockFactory;
}

