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
#include "configreader.h"
#include "inspircd.h"

/* $ModDesc: Provides support for /KNOCK and mode +K */

/** Handles the /KNOCK command
 */
class cmd_knock : public command_t
{
 public:
	cmd_knock (InspIRCd* Instance) : command_t(Instance,"KNOCK", 0, 2)
	{
		this->source = "m_knock.so";
		syntax = "<channel> <reason>";
	}
	
	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		chanrec* c = ServerInstance->FindChan(parameters[0]);

		if (!c)
		{
			user->WriteServ("401 %s %s :No such channel",user->nick, parameters[0]);
			return CMD_FAILURE;
		}

		std::string line = "";

		if (c->IsModeSet('K'))
		{
			user->WriteServ("480 %s :Can't KNOCK on %s, +K is set.",user->nick, c->name);
			return CMD_FAILURE;
		}

		for (int i = 1; i < pcnt - 1; i++)
		{
			line = line + std::string(parameters[i]) + " ";
		}
		line = line + std::string(parameters[pcnt-1]);

		if (c->modes[CM_INVITEONLY])
		{
			c->WriteChannelWithServ((char*)ServerInstance->Config->ServerName,  "NOTICE %s :User %s is KNOCKing on %s (%s)", c->name, user->nick, c->name, line.c_str());
			user->WriteServ("NOTICE %s :KNOCKing on %s",user->nick,c->name);
			return CMD_SUCCESS;
		}
		else
		{
			user->WriteServ("480 %s :Can't KNOCK on %s, channel is not invite only so knocking is pointless!",user->nick, c->name);
			return CMD_FAILURE;
		}

		return CMD_SUCCESS;
	}
};

/** Handles channel mode +K
 */
class Knock : public ModeHandler
{
 public:
	Knock(InspIRCd* Instance) : ModeHandler(Instance, 'K', 0, 0, false, MODETYPE_CHANNEL, false) { }

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
	ModuleKnock(InspIRCd* Me) : Module::Module(Me)
	{
		
		kn = new Knock(ServerInstance);
		ServerInstance->AddMode(kn, 'K');
		mycommand = new cmd_knock(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
	}

	virtual ~ModuleKnock()
	{
		ServerInstance->Modes->DelMode(kn);
		DELETE(kn);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_COMMON|VF_VENDOR,API_VERSION);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleKnock(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleKnockFactory;
}

