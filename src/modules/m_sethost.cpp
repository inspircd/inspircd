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

/* $ModDesc: Provides support for the SETHOST command */

Server *Srv;
	 
void handle_sethost(char **parameters, int pcnt, userrec *user)
{
	for (int x = 0; x < strlen(parameters[0]); x++)
	{
		if (((tolower(parameters[0][x]) < 'a') || (tolower(parameters[0][x]) > 'z')) && (parameters[0][x] != '.'))
		{
			if (((parameters[0][x] < '0') || (parameters[0][x]> '9')) && (parameters[0][x] != '-'))
			{
				Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+" :*** Invalid characters in hostname");
				return;
			}
		}
	}
	Srv->ChangeHost(user,parameters[0]);
	Srv->SendOpers(std::string(user->nick)+" used SETHOST to change their displayed host to "+std::string(parameters[0]));
}


class ModuleSetHost : public Module
{
 public:
	ModuleSetHost()
	{
		Srv = new Server;
		Srv->AddCommand("SETHOST",handle_sethost,'o',1);
	}
	
	virtual ~ModuleSetHost()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSetHostFactory : public ModuleFactory
{
 public:
	ModuleSetHostFactory()
	{
	}
	
	~ModuleSetHostFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleSetHost;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetHostFactory;
}

