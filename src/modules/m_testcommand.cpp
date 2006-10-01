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
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "dns.h"
#include "inspircd.h"

/* $ModDesc: Povides a proof-of-concept test /WOOT command */



class MyV6Resolver : public Resolver
{
	bool fw;
 public:
	MyV6Resolver(const std::string &source, bool forward) : Resolver(ServerInstance, source, forward ? DNS_QUERY_AAAA : DNS_QUERY_PTR6)
	{
		fw = forward;
	}

	virtual void OnLookupComplete(const std::string &result)
	{
		ServerInstance->Log(DEBUG,"*** RESOLVER COMPLETED %s LOOKUP, IP IS: '%s'",fw ? "FORWARD" : "REVERSE", result.c_str());
	}

	virtual void OnError(ResolverError e, const std::string &errormessage)
	{
		ServerInstance->Log(DEBUG,"*** RESOLVER GOT ERROR: %d: %s",e,errormessage.c_str());
	}
};


	 
class cmd_woot : public command_t
{
 public:
	cmd_woot (InspIRCd* Instance) : command_t(Instance,"WOOT", 0, 0)
	{
		this->source = "m_testcommand.so";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		/* We dont have to worry about deleting 'r', the core will
		 * do it for us as required.*/

		try
		{
			MyV6Resolver* r = new MyV6Resolver("shake.stacken.kth.se", true);
			ServerInstance->AddResolver(r);
			r = new MyV6Resolver("2001:6b0:1:ea:202:a5ff:fecd:13a6", false);
			ServerInstance->AddResolver(r);
		}
		catch (ModuleException& e)
		{
			ServerInstance->Log(DEBUG,"Danger, will robinson! There was an exception: %s",e.GetReason());
		}

		return CMD_FAILURE;
	}
};

class ModuleTestCommand : public Module
{
	cmd_woot* newcommand;
 public:
	ModuleTestCommand(InspIRCd* Me)
		: Module::Module(Me)
	{
		
		// Create a new command:
		// command will be called /WOOT, and will
		// call handle_woot when triggered, the
		// 0 in the modes parameter signifies that
		// anyone can issue the command, and the
		// command takes only one parameter.
		newcommand = new cmd_woot(ServerInstance);
		ServerInstance->AddCommand(newcommand);
	}

	void Implements(char* List)
	{
		List[I_OnUserJoin] = 1;
	}

	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
	}
	
	virtual ~ModuleTestCommand()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR, API_VERSION);
	}
};


class ModuleTestCommandFactory : public ModuleFactory
{
 public:
	ModuleTestCommandFactory()
	{
	}
	
	~ModuleTestCommandFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleTestCommand(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleTestCommandFactory;
}

