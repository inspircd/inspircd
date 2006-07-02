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

#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Sends a numeric on connect which cripples a common type of trojan/spambot */

class ModuleAntiBear : public Module
{
 private:
	 
	 Server *Srv;
 public:
	ModuleAntiBear(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
	}
	
	virtual ~ModuleAntiBear()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}

	void Implements(char* List)
	{
		List[I_OnUserConnect] = 1;
	}
	
	virtual void OnUserConnect(userrec* user)
	{
		WriteServ(user->fd,"439 %s :This server has anti-spambot mechanisms enabled.", user->nick);
		WriteServ(user->fd,"931 %s :Spambots, trojans, and malicious botnets are", user->nick);
		WriteServ(user->fd,"437 %s :NOT WELCOME HERE. Please take your war elsewhere.", user->nick);
	}
};

class ModuleAntiBearFactory : public ModuleFactory
{
 public:
	ModuleAntiBearFactory()
	{
	}
	
	~ModuleAntiBearFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleAntiBear(Me);
	}
	
};


//
// The "C" linkage factory0() function creates the ModuleAntiBearFactory
// class for this library
//

extern "C" void * init_module( void )
{
	return new ModuleAntiBearFactory;
}

