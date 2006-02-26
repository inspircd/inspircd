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

static Server *Srv;
	 
class cmd_samode : public command_t
{
 public:
	cmd_samode () : command_t("SAMODE", 'o', 2)
	{
		this->source = "m_samode.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		/*
		 * Handles an SAMODE request. Notifies all +s users.
	 	 */
		int n=0;
		std::string result;
		Srv->Log(DEBUG,"SAMODE: Being handled");
		Srv->SendMode(parameters,pcnt,user);
		Srv->Log(DEBUG,"SAMODE: Modechange handled");
		result = std::string(user->nick) + std::string(" used SAMODE ");
	  	while (n<pcnt)
		{
			result=result + std::string(" ") + std::string(parameters[n]);
			n++;
		}
		Srv->SendOpers(result);
	}
};

class ModuleSaMode : public Module
{
	cmd_samode*	mycommand;
 public:
	ModuleSaMode(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_samode();
		Srv->AddCommand(mycommand);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSaMode(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSaModeFactory;
}
