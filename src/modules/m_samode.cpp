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

/*
 * SAMODE module for InspIRCd
 *  Co authored by Brain and w00t
 *
 *  Syntax: /SAMODE <#chan/nick> +/-<modes> [parameters for modes]
 * 
 */

/* $ModDesc: Povides more advanced UnrealIRCd SAMODE command */

/*
 * ToDo:
 *   Err... not a lot really.
 */ 

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"



	 
class cmd_samode : public command_t
{
 public:
	cmd_samode (InspIRCd* Instance) : command_t(Instance,"SAMODE", 'o', 2)
	{
		this->source = "m_samode.so";
		syntax = "<target> <modes> {<mode-parameters>}";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		/*
		 * Handles an SAMODE request. Notifies all +s users.
	 	 */

		userrec* n = new userrec(ServerInstance);
		n->SetFd(FD_MAGIC_NUMBER);
		ServerInstance->SendMode(parameters,pcnt,n);
		delete n;

		if (ServerInstance->Modes->GetLastParse().length())
			ServerInstance->WriteOpers(std::string(user->nick)+" used SAMODE: "+ServerInstance->Modes->GetLastParse());

		return CMD_SUCCESS;
	}
};

class ModuleSaMode : public Module
{
	cmd_samode*	mycommand;
 public:
	ModuleSaMode(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_samode(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSaMode()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,2,2,VF_VENDOR,API_VERSION);
	}
};


class ModuleSaModeFactory : public ModuleFactory
{
 public:
	ModuleSaModeFactory()
	{
	}
	
	~ModuleSaModeFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSaMode(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSaModeFactory;
}
