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

/* $ModDesc: Provides support for unreal-style SAPART command */

Server *Srv;

class cmd_sapart : public command_t
{
 public:
	cmd_sapart () : command_t("SAPART", 'o', 2)
	{
		this->source = "m_sapart.so";
	}
	 
	void Handle (char **parameters, int pcnt, userrec *user)
	{
		userrec* dest = Srv->FindNick(std::string(parameters[0]));
		if (dest)
		{
			if (!IsValidChannelName(parameters[1]))
			{
				Srv->SendTo(NULL,user,"NOTICE "+std::string(user->nick)+" :*** Invalid characters in channel name");
				return;
			}

			Srv->SendOpers(std::string(user->nick)+" used SAPART to make "+std::string(dest->nick)+" part "+parameters[1]);
			Srv->PartUserFromChannel(dest,std::string(parameters[1]),std::string(dest->nick));
		}
	}
};


class ModuleSapart : public Module
{
	cmd_sapart*	mycommand;
 public:
	ModuleSapart(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_sapart();
		Srv->AddCommand(mycommand);
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

