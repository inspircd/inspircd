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
#include "modules.h"
#include "inspircd.h"

/** Handle /DEVOICE
 */	 
class cmd_devoice : public command_t
{
 public:
	cmd_devoice (InspIRCd* Instance) : command_t(Instance,"DEVOICE", 0, 1)
	{
		this->source = "m_devoice.so";
		syntax = "<channel>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		chanrec* c = ServerInstance->FindChan(parameters[0]);
		if (c && c->HasUser(user))
		{
			const char* modes[3];
			modes[0] = parameters[0];
			modes[1] = "-v";
			modes[2] = user->nick;

			ServerInstance->SendMode(modes,3,user);

			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
	}
};

class ModuleDeVoice : public Module
{
	cmd_devoice *mycommand;
 public:
	ModuleDeVoice(InspIRCd* Me) : Module::Module(Me)
	{
		
		mycommand = new cmd_devoice(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleDeVoice()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleDeVoice(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleDeVoiceFactory;
}
