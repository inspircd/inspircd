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

/* $ModDesc: Provides support for unreal-style SAPART command */




class cmd_sapart : public command_t
{
 public:
 cmd_sapart (InspIRCd* Instance) : command_t(Instance,"SAPART", 'o', 2)
	{
		this->source = "m_sapart.so";
		syntax = "<nick> <channel>";
	}
	 
	void Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* dest = ServerInstance->FindNick(parameters[0]);
		chanrec* channel = ServerInstance->FindChan(parameters[1]);
		if (dest && channel)
		{
			if (ServerInstance->IsUlined(dest->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return;
			}
			ServerInstance->WriteOpers(std::string(user->nick)+" used SAPART to make "+dest->nick+" part "+parameters[1]);
			if (!channel->PartUser(dest, dest->nick))
				delete channel;
		}
	}
};


class ModuleSapart : public Module
{
	cmd_sapart*	mycommand;
 public:
	ModuleSapart(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_sapart(ServerInstance);
		ServerInstance->AddCommand(mycommand);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSapart(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSapartFactory;
}

