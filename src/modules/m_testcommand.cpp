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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Povides a proof-of-concept test /WOOT command */

static Server *Srv;
	 
class cmd_woot : public command_t
{
 public:
	cmd_woot () : command_t("WOOT", 0, 0)
	{
		this->source = "m_testcommand.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		Srv->Log(DEBUG,"woot handler");
		// Here is a sample of how to send servermodes. Note that unless remote
		// servers in your net are u:lined, they may reverse this, but its a
		// quick and effective modehack.
		
		// NOTE: DO NOT CODE LIKE THIS!!! This has no checks and can send
		// invalid or plain confusing mode changes, code some checking!
		char* modes[3];
		modes[0] = "#chatspike";
		modes[1] = "+o";
		modes[2] = user->nick;
		
		// run the mode change, send numerics (such as "no such channel") back
		// to "user".
		Srv->SendMode(modes,3,user);
	}
};

class ModuleTestCommand : public Module
{
	cmd_woot* newcommand;
 public:
	ModuleTestCommand(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		// Create a new command:
		// command will be called /WOOT, and will
		// call handle_woot when triggered, the
		// 0 in the modes parameter signifies that
		// anyone can issue the command, and the
		// command takes only one parameter.
		newcommand = new cmd_woot();
		Srv->AddCommand(newcommand);
	}

	void Implements(char* List)
	{
		List[I_OnUserJoin] = 1;
	}

	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
	}
	
	virtual ~ModuleTestCommand()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleTestCommandFactory : public ModuleFactory
{
 public:
	ModuleTestCommandFactory()
	{
	}
	
	~ModuleTestCommandFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleTestCommand(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleTestCommandFactory;
}

