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
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Povides a proof-of-concept test /WOOT command */

static Server *Srv;

enum HttpState
{
	HTTP_LISTEN = 0,
	HTTP_SERVE = 1
};

class HttpSocket : public InspSocket
{
 public:

	HttpSocket(std::string host, int port, bool listening, unsigned long maxtime) : InspSocket(host, port, listening, maxtime)
	{
	}

	HttpSocket(int newfd, char* ip) : InspSocket(newfd, ip)
	{
	}
};

class ModuleHttp : public Module
{
 public:

	void ReadConfig()
	{
		ConfigReader c;
	}

	ModuleHttp(Server* Me)
		: Module::Module(Me)
	{
		ReadConfig();
	}

	void Implements(char* List)
	{
		List[I_OnEvent] = List[I_OnRequest] = 1;
	}

	virtual ~ModuleHttp()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleHttpFactory : public ModuleFactory
{
 public:
	ModuleHttpFactory()
	{
	}
	
	~ModuleHttpFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleHttp(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleHttpFactory;
}

