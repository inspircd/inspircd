/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *		       E-mail:
 *		<brain@chatspike.net>
 *	   	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
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

/* $ModDesc: Provides support for unreal-style SAJOIN command */

static Server *Srv;
extern InspIRCd* ServerInstance;

class cmd_sajoin : public command_t
{
 public:
	cmd_sajoin() : command_t("SAJOIN", 'o', 2)
	{
		this->source = "m_sajoin.so";
		syntax = "<nick> <channel>";
	}

	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* dest = Srv->FindNick(std::string(parameters[0]));
		if (dest)
		{
			if (Srv->IsUlined(dest->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return;
			}
			if (!IsValidChannelName(parameters[1]))
			{
				/* we didn't need to check this for each character ;) */
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in channel name");
				return;
			}

			Srv->SendOpers(std::string(user->nick)+" used SAJOIN to make "+std::string(dest->nick)+" join "+parameters[1]);
			chanrec::JoinUser(ServerInstance, dest, parameters[1], true);
		}
	}
};

class ModuleSajoin : public Module
{
	cmd_sajoin*	mycommand;
 public:
	ModuleSajoin(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_sajoin();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleSajoin()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSajoinFactory : public ModuleFactory
{
 public:
	ModuleSajoinFactory()
	{
	}
	
	~ModuleSajoinFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleSajoin(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSajoinFactory;
}

