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

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Allows opers to set their idle time */

Server *Srv = NULL;
	 
void handle_setidle(char **parameters, int pcnt, userrec *user)
{
	user->idle_lastmsg = time(NULL) - atoi(parameters[0]);
	// minor tweak - we cant have signon time shorter than our idle time!
	if (user->signon > user->idle_lastmsg)
		user->signon = user->idle_lastmsg;
	Srv->SendOpers(std::string(user->nick)+" used SETIDLE to set their idle time to "+std::string(parameters[0])+" seconds");
	WriteServ(user->fd,"944 %s :Idle time set.",user->nick);
}


class ModuleSetIdle : public Module
{
 public:
	ModuleSetIdle()
	{
		Srv = new Server;
		Srv->AddCommand("SETIDLE",handle_setidle,'o',1,"m_setidle.so");
	}
	
	virtual ~ModuleSetIdle()
	{
		if (Srv)
			delete Srv;
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
	
	virtual Module * CreateModule()
	{
		return new ModuleSetIdle;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetIdleFactory;
}

