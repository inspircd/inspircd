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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Provides support for SANICK command */

extern InspIRCd* ServerInstance;
static Server *Srv;

class cmd_sanick : public command_t
{
 public:
	cmd_sanick () : command_t("SANICK", 'o', 2)
	{
		this->source = "m_sanick.so";
		syntax = "<nick> <new-nick>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* source = ServerInstance->FindNick(parameters[0]);
		if (source)
		{
			if (Srv->IsUlined(source->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return;
			}
			if (Srv->IsNick(std::string(parameters[1])))
			{
				// FIX by brain: Cant use source->nick here because if it traverses a server link then
				// source->nick becomes invalid as the object data moves in memory.
				ServerInstance->WriteOpers(std::string(user->nick)+" used SANICK to change "+std::string(parameters[0])+" to "+parameters[1]);
				if (!source->ForceNickChange(parameters[1]))
				{
					/* We couldnt change the nick */
					userrec::QuitUser(ServerInstance, source, "Nickname collision");
					return;
				}
			}
		}
	}
};


class ModuleSanick : public Module
{
	cmd_sanick*	mycommand;
 public:
	ModuleSanick(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_sanick();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleSanick()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSanickFactory : public ModuleFactory
{
 public:
	ModuleSanickFactory()
	{
	}
	
	~ModuleSanickFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSanick(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSanickFactory;
}

