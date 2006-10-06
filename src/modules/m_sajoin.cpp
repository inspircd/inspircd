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

/* $ModDesc: Provides support for unreal-style SAJOIN command */

/** Handle /SAJOIN
 */
class cmd_sajoin : public command_t
{
 public:
 cmd_sajoin (InspIRCd* Instance) : command_t(Instance,"SAJOIN", 'o', 2)
	{
		this->source = "m_sajoin.so";
		syntax = "<nick> <channel>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		userrec* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			if (ServerInstance->ULine(dest->server))
			{
				user->WriteServ("990 %s :Cannot use an SA command on a u-lined client",user->nick);
				return CMD_FAILURE;
			}
			if (!ServerInstance->IsChannel(parameters[1]))
			{
				/* we didn't need to check this for each character ;) */
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in channel name");
				return CMD_FAILURE;
			}

			chanrec::JoinUser(ServerInstance, dest, parameters[1], true);

			/* Fix for dotslasher and w00t - if the join didnt succeed, return CMD_FAILURE so that it doesnt propogate */
			chanrec* n = ServerInstance->FindChan(parameters[1]);
			if (n)
			{
				if (n->HasUser(dest))
				{
					ServerInstance->WriteOpers(std::string(user->nick)+" used SAJOIN to make "+std::string(dest->nick)+" join "+parameters[1]);
					return CMD_SUCCESS;
				}
				else
				{
					user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Could not join "+std::string(dest->nick)+" to "+parameters[1]+" (User is probably banned, or blocking modes)");
					return CMD_FAILURE;
				}
			}
			else
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Could not join "+std::string(dest->nick)+" to "+parameters[1]);
				return CMD_FAILURE;
			}
		}

		return CMD_SUCCESS;
	}
};

class ModuleSajoin : public Module
{
	cmd_sajoin*	mycommand;
 public:
	ModuleSajoin(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		mycommand = new cmd_sajoin(ServerInstance);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleSajoin()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR,API_VERSION);
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
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSajoin(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSajoinFactory;
}

