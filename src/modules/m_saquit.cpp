/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

/*
 * SAQUIT module for InspIRCd
 *  Author: w00t
 *  Version: 1.0.0.0
 *
 *  Syntax: /SAQUIT <user> [reason]
 *  Makes it appear as though <user> has /quit with [reason]
 *  
 */

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for an SAQUIT command, exits user with a reason */

Server *Srv;

class cmd_saquit : public command_t
{
 public:
	cmd_saquit () : command_t("SAQUIT",'o',2)
	{
		this->source = "m_saquit.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		userrec* dest = Srv->FindNick(std::string(parameters[0]));
		if (dest)
		{
			std::string line = "";
			for (int i = 1; i < pcnt - 1; i++)
			{
				line = line + std::string(parameters[i]) + " ";
			}
			line = line + std::string(parameters[pcnt-1]);
		
			Srv->SendOpers(std::string(user->nick)+" used SAQUIT to make "+std::string(dest->nick)+" quit with a reason of "+line);
			Srv->QuitUser(dest, line);
		}
	}
};

class ModuleSaquit : public Module
{
	cmd_saquit*	mycommand;
 public:
	ModuleSaquit(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_saquit();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleSaquit()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSaquitFactory : public ModuleFactory
{
 public:
	ModuleSaquitFactory()
	{
	}
	
	~ModuleSaquitFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSaquit(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSaquitFactory;
}

