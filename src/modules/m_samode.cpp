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


extern InspIRCd* ServerInstance;
	 
class cmd_samode : public command_t
{
 public:
	cmd_samode () : command_t("SAMODE", 'o', 2)
	{
		this->source = "m_samode.so";
		syntax = "<target> <modes> {<mode-parameters>}";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		/*
		 * Handles an SAMODE request. Notifies all +s users.
	 	 */
		std::string result;
		log(DEBUG,"SAMODE: Being handled");
		userrec* n = new userrec(ServerInstance);
		n->fd = FD_MAGIC_NUMBER;
		ServerInstance->SendMode(parameters,pcnt,n);
		delete n;
		log(DEBUG,"SAMODE: Modechange handled");
		result = std::string(user->nick);
		result.append(" used SAMODE");
	  	for (int n = 0; n < pcnt; n++)
		{
			result.append(" ");
			result.append(parameters[n]);
		}
		ServerInstance->WriteOpers(result);
	}
};

class ModuleSaMode : public Module
{
	cmd_samode*	mycommand;
 public:
	ModuleSaMode(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_samode();
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSaMode()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,2,2,VF_VENDOR);
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
