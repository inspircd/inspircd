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
	 
void handle_saquit(char **parameters, int pcnt, userrec *user)
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


class ModuleSaquit : public Module
{
 public:
	ModuleSaquit()
	{
		Srv = new Server;
		Srv->AddCommand("SAQUIT",handle_saquit,'o',2,"m_saquit.so");
	}
	
	virtual ~ModuleSaquit()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,0);
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
	
	virtual Module * CreateModule()
	{
		return new ModuleSaquit;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSaquitFactory;
}

