/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
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

#include <stdio.h>
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for RFC 1413 ident lookups */

Server *Srv;
	 
class ModuleIdent : public Module
{
 public:
	ModuleIdent()
	{
		Srv = new Server;
	}

	virtual void OnUserRegister(userrec* user)
	{
	}

	virtual bool OnUserReady(userrec* user)
	{
		return true;
	}
	
	virtual ~ModuleIdent()
	{
		delete Srv;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleIdentFactory : public ModuleFactory
{
 public:
	ModuleIdentFactory()
	{
	}
	
	~ModuleIdentFactory()
	{
	}
	
	virtual Module * CreateModule()
	{
		return new ModuleIdent;
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleIdentFactory;
}

