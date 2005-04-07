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

/* $ModDesc: Provides support for the CHGHOST command */

Server *Srv;
	 
void handle_chghost(char **parameters, int pcnt, userrec *user)
{
	for (int x = 0; x < strlen(parameters[1]); x++)
	{
		if (((tolower(parameters[1][x]) < 'a') || (tolower(parameters[1][x]) > 'z')) && (parameters[1][x] != '.'))
		{
			if (((parameters[1][x] < '0') || (parameters[1][x]> '9')) && (parameters[1][x] != '-'))
			{
				Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+" :*** Invalid characters in hostname");
				return;
			}
		}
	}	userrec* dest = Srv->FindNick(std::string(parameters[0]));
	if (dest)
	{
		Srv->ChangeHost(dest,parameters[1]);
		Srv->SendOpers(std::string(user->nick)+" used CHGHOST to make the displayed host of "+std::string(dest->nick)+" become "+std::string(parameters[1]));
	}
}


class ModuleChgHost : public Module
{
 public:
	ModuleChgHost()
	{
		Srv = new Server;
		Srv->AddCommand("CHGHOST",handle_chghost,'o',2,"m_chghost.so");
	}
	
	virtual ~ModuleChgHost()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,0);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleChgHostFactory : public ModuleFactory
{
 public:
	ModuleChgHostFactory()
	{
	}
	
	~ModuleChgHostFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleChgHost;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChgHostFactory;
}

