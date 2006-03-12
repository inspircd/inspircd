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
		std::string line = "";

		if (c->IsCustomModeSet('K'))
		{
			WriteServ(user->fd,"480 %s :Can't KNOCK on %s, +K is set.",user->nick, c->name);
			return;
		}

		for (int i = 1; i < pcnt - 1; i++)
		{
			line = line + std::string(parameters[i]) + " ";
		}
		line = line + std::string(parameters[pcnt-1]);

		if (c->custom_modes[CM_INVITEONLY])
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

class ModuleKnock : public Module
{
	cmd_knock* mycommand;
 public:
	ModuleKnock(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Srv->AddExtendedMode('K',MT_CHANNEL,false,0,0);
		mycommand = new cmd_knock();
		Srv->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = List[I_OnExtendedMode] = 1;
	}

        virtual void On005Numeric(std::string &output)
        {
		InsertMode(output,"K",4);
        }

	virtual ~ModuleKnock()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_STATIC|VF_VENDOR);
	}
	
	virtual int OnExtendedMode(userrec* user, void* target, char modechar, int type, bool mode_on, string_list &params)
	{
		// check if this is our mode character...
		if ((modechar == 'K') && (type == MT_CHANNEL))
  		{
			return 1;
		}
		else
		{
			return 0;
		}
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

