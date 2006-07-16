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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for the SETHOST command */

static Server *Srv;

class cmd_sethost : public command_t
{
 public:
	cmd_sethost() : command_t("SETHOST",'o',1)
	{
		this->source = "m_sethost.so";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (strlen(parameters[0]) > 64)
		{
			WriteServ(user->fd,"NOTICE %s :*** SETHOST: Host too long",user->nick);
			return;
		}
		for (unsigned int x = 0; x < strlen(parameters[0]); x++)
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
};


class ModuleSetHost : public Module
{
	cmd_sethost*	mycommand;
 public:
	ModuleSetHost(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_sethost();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleSetHost()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
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
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSetHost(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetHostFactory;
}

