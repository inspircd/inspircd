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

/** A test resolver class for IPV6
 */
class MyV6Resolver : public Resolver
{
	bool fw;
 public:
	MyV6Resolver(Module* me, const std::string &source, bool forward) : Resolver(ServerInstance, source, forward ? DNS_QUERY_AAAA : DNS_QUERY_PTR6, me)
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

/** Handle /WOOT
 */
class cmd_woot : public command_t
{
	Module* Creator;
 public:
	/* Command 'woot', takes no parameters and needs no special modes */
	cmd_woot (InspIRCd* Instance, Module* maker) : command_t(Instance,"WOOT", 0, 0), Creator(maker)
	{
		this->source = "m_testcommand.so";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		/* We dont have to worry about deleting 'r', the core will
		 * do it for us as required.*/
		try
		{
			MyV6Resolver* r = new MyV6Resolver(Creator, "shake.stacken.kth.se", true);
			ServerInstance->AddResolver(r);
			r = new MyV6Resolver(Creator, "2001:6b0:1:ea:202:a5ff:fecd:13a6", false);
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
		
		// Create a new command
		newcommand = new cmd_woot(ServerInstance, this);
		ServerInstance->AddCommand(newcommand);
	}

	void Implements(char* List)
	{
		List[I_OnUserJoin] = 1;
	}

	virtual void OnUserJoin(userrec* user, chanrec* channel)
	{
		/* This is an example, we do nothing here */
	}
	
	virtual ~ModuleTestCommand()
	{
		delete newcommand;
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
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

