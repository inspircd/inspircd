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

/* $ModDesc: Provides support for unreal-style SAPART command */

Server *Srv;
	 
void handle_sapart(char **parameters, int pcnt, userrec *user)
{
	userrec* dest = Srv->FindNick(std::string(parameters[0]));
	if (dest)
	{
		for (unsigned int x = 0; x < strlen(parameters[1]); x++)
		{
				if ((parameters[1][0] != '#') || (parameters[1][x] == ' ') || (parameters[1][x] == ','))
				{
					Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+" :*** Invalid characters in channel name");
					return;
				}
		}

		Srv->SendOpers(std::string(user->nick)+" used SAPART to make "+std::string(dest->nick)+" part "+parameters[1]);
		Srv->PartUserFromChannel(dest,std::string(parameters[1]),std::string(dest->nick));
	}
}


class ModuleSapart : public Module
{
 public:
	ModuleSapart(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		Srv->AddCommand("SAPART",handle_sapart,'o',2,"m_sapart.so");
	}
	
	virtual ~ModuleSapart()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSapartFactory : public ModuleFactory
{
 public:
	ModuleSapartFactory()
	{
	}
	
	~ModuleSapartFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSapart(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSapartFactory;
}

