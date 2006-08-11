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
#include "inspircd.h"

/* $ModDesc: Provides support for the CHGHOST command */



class cmd_chghost : public command_t
{
 public:
 cmd_chghost (InspIRCd* Instance) : command_t(Instance,"CHGHOST",'o',2)
	{
		this->source = "m_chghost.so";
		syntax = "<nick> <newhost>";
	}
	 
	void Handle(const char** parameters, int pcnt, userrec *user)
	{
		const char * x = parameters[1];

		for (; *x; x++)
		{
			if (((tolower(*x) < 'a') || (tolower(*x) > 'z')) && (*x != '.'))
			{
				if (((*x < '0') || (*x > '9')) && (*x != '-'))
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in hostname");
					return;
				}
			}
		}
		if ((parameters[1] - x) > 63)
		{
			user->WriteServ("NOTICE %s :*** CHGHOST: Host too long",user->nick);
			return;
		}
		userrec* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			if ((dest->ChangeDisplayedHost(parameters[1])) && (!ServerInstance->IsUlined(user->server)))
			{
				// fix by brain - ulines set hosts silently
				ServerInstance->WriteOpers(std::string(user->nick)+" used CHGHOST to make the displayed host of "+dest->nick+" become "+parameters[1]);
			}
		}
	}
};


class ModuleChgHost : public Module
{
	cmd_chghost* mycommand;
 public:
	ModuleChgHost(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_chghost(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
	}
	
	virtual ~ModuleChgHost()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,2,0,1,VF_VENDOR);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleChgHost(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChgHostFactory;
}

