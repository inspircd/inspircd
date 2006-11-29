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
#include "inspircd.h"

/* $ModDesc: Provides support for SANICK command */

/** Handle /SANICK
 */
class cmd_sanick : public command_t
{
 public:
 cmd_sanick (InspIRCd* Instance) : command_t(Instance,"SANICK", 'o', 2)
	{
		this->source = "m_sanick.so";
		syntax = "<nick> <new-nick>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* source = ServerInstance->FindNick(parameters[0]);
		if (source)
		{
			if (ServerInstance->ULine(source->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return CMD_FAILURE;
			}
			std::string oldnick = user->nick;
			if (ServerInstance->IsNick(parameters[1]))
			{
				if (source->ForceNickChange(parameters[1]))
				{
					ServerInstance->WriteOpers(oldnick+" used SANICK to change "+std::string(parameters[0])+" to "+parameters[1]);
					return CMD_SUCCESS;
				}
				else
				{
					/* We couldnt change the nick */
					ServerInstance->WriteOpers(oldnick+" failed SANICK (from "+std::string(parameters[0])+" to "+parameters[1]+")");
					return CMD_FAILURE;
				}
			}
			else
			{
				user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick, parameters[1]);
			}

			return CMD_FAILURE;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** No such nickname: '%s'", user->nick, parameters[0]);
		}

		return CMD_FAILURE;
	}
};


class ModuleSanick : public Module
{
	cmd_sanick*	mycommand;
 public:
	ModuleSanick(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_sanick(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSanick()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleSanickFactory : public ModuleFactory
{
 public:
	ModuleSanickFactory()
	{
	}
	
	~ModuleSanickFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSanick(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSanickFactory;
}

