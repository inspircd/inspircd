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

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Allows opers to set their idle time */

Server *Srv = NULL;

class cmd_setidle : public command_t
{
 public:
	cmd_setidle () : command_t("SETIDLE", 'o', 1)
	{
		this->source = "m_setidle.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		if (atoi(parameters[0]) < 1)
		{
			WriteServ(user->fd,"948 %s :Invalid idle time.",user->nick);
			return;
		}
		user->idle_lastmsg = time(NULL) - atoi(parameters[0]);
		// minor tweak - we cant have signon time shorter than our idle time!
		if (user->signon > user->idle_lastmsg)
			user->signon = user->idle_lastmsg;
		Srv->SendOpers(std::string(user->nick)+" used SETIDLE to set their idle time to "+std::string(parameters[0])+" seconds");
		WriteServ(user->fd,"944 %s :Idle time set.",user->nick);
	}
};


class ModuleSetIdle : public Module
{
	cmd_setidle*	mycommand;
 public:
	ModuleSetIdle(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_setidle();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleSetIdle()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSetIdleFactory : public ModuleFactory
{
 public:
	ModuleSetIdleFactory()
	{
	}
	
	~ModuleSetIdleFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSetIdle(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetIdleFactory;
}

