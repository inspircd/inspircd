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
#include "inspircd.h"

/* $ModDesc: Provides support for an SAQUIT command, exits user with a reason */




class cmd_saquit : public command_t
{
 public:
 cmd_saquit (InspIRCd* Instance) : command_t(Instance,"SAQUIT",'o',2)
	{
		this->source = "m_saquit.so";
		syntax = "<nick> <reason>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			if (ServerInstance->IsUlined(dest->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return;
			}
			std::string line = "";
			for (int i = 1; i < pcnt - 1; i++)
			{
				line = line + std::string(parameters[i]) + " ";
			}
			line = line + std::string(parameters[pcnt-1]);
		
			ServerInstance->WriteOpers(std::string(user->nick)+" used SAQUIT to make "+std::string(dest->nick)+" quit with a reason of "+line);
			userrec::QuitUser(ServerInstance, dest, line);
		}
	}
};

class ModuleSaquit : public Module
{
	cmd_saquit*	mycommand;
 public:
	ModuleSaquit(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_saquit(ServerInstance);
		ServerInstance->AddCommand(mycommand);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSaquit(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSaquitFactory;
}

