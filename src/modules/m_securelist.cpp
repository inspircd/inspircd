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
 
#include "users.h"
#include "channels.h"
#include "modules.h"

#include <vector>
#include "inspircd.h"

/* $ModDesc: A module overriding /list, and making it safe - stop those sendq problems. */

class ModuleSecureList : public Module
{
 private:
	 
 public:
	ModuleSecureList(InspIRCd* Me) : Module::Module(Me)
	{
		
	}
 
	virtual ~ModuleSecureList()
	{
	}
 
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
 
	void Implements(char* List)
	{
		List[I_OnPreCommand] = List[I_On005Numeric] = 1;
	}

	/*
	 * OnPreCommand()
	 *   Intercept the LIST command.
	 */ 
	virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated)
	{
		/* If the command doesnt appear to be valid, we dont want to mess with it. */
		if (!validated)
			return 0;
 
		if ((command == "LIST") && (ServerInstance->Time() < (user->signon+60)) && (!*user->oper))
		{
			user->WriteServ("NOTICE %s :*** You cannot list within the first minute of connecting. Please try again later.",user->nick);
			/* Some crap clients (read: mIRC, various java chat applets) muck up if they don't
			 * receive these numerics whenever they send LIST, so give them an empty LIST to mull over.
			 */
			user->WriteServ("321 %s Channel :Users Name",user->nick);
			user->WriteServ("323 %s :End of channel list.",user->nick);
			return 1;
		}
		return 0;
	}

	virtual void On005Numeric(std::string &output)
	{
		output.append(" SECURELIST");
	}

	virtual Priority Prioritize()
	{
		return (Priority)ServerInstance->PriorityBefore("m_safelist.so");
	}

};
 
 
 
/******************************************************************************************************/
 
class ModuleSecureListFactory : public ModuleFactory
{
 public:
	ModuleSecureListFactory()
	{
	}
 
	~ModuleSecureListFactory()
	{
	}
 
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSecureList(Me);
	}
 
};
 
extern "C" void * init_module( void )
{
	return new ModuleSecureListFactory;
}
