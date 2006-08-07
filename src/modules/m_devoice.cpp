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
 * DEVOICE module for InspIRCd
 *  Syntax: /DEVOICE <#chan>
 */

/* $ModDesc: Provides voiced users with the ability to devoice themselves. */

#include <stdio.h>
#include "users.h"
#include "channels.h"
#include "helperfuncs.h"
#include "modules.h"

static Server *Srv;
	 
class cmd_devoice : public command_t
{
 public:
	cmd_devoice () : command_t("DEVOICE", 0, 1)
	{
		this->source = "m_devoice.so";
		syntax = "<channel>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		/*
		 * NOTE: DO NOT CODE LIKE THIS!!! This has no checks and can send
		 * invalid or plain confusing mode changes, code some checking!
		 *
		 * - I'm not aware what checking I need, so for now... be supreme evil.
		 */
		const char* modes[3];
		modes[0] = parameters[0];
		modes[1] = "-v";
		modes[2] = user->nick;

		Srv->SendMode(modes,3,user);
	}
};

class ModuleDeVoice : public Module
{
	cmd_devoice *mycommand;
 public:
	ModuleDeVoice(Server* Me) : Module::Module(Me)
	{
		log(DEBUG,"ModuleDeVoice constructor");
		Srv = Me;
		mycommand = new cmd_devoice();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleDeVoice()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}
};


class ModuleDeVoiceFactory : public ModuleFactory
{
 public:
	ModuleDeVoiceFactory()
	{
	}
	
	~ModuleDeVoiceFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		log(DEBUG,"ModuleDevoiceFactory::CreateModule()");
		return new ModuleDeVoice(Me);
	}
	
};


extern "C" void * crud_u_like( void )
{
	return new ModuleDeVoiceFactory;
}
