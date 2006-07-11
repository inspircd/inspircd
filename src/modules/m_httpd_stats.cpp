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
#include "inspsocket.h"
#include "helperfuncs.h"
#include "httpd.h"

/* $ModDesc: Provides statistics over HTTP via m_httpd.so */

class ModuleHttpStats : public Module
{
	int port;
	std::string host;
	std::string bindip;
	std::string indexfile;

	FileReader index;

	HttpSocket* http;

 public:

	void ReadConfig()
	{
	}

	ModuleHttpStats(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		ReadConfig();
	}

	void OnEvent(Event* event)
	{
		if (event->GetID() == "httpd_url")
		{
			log(DEBUG,"HTTP URL!");
		}
	}

	char* OnRequest(Request* request)
	{
		return NULL;
	}

	void Implements(char* List)
	{
		List[I_OnEvent] = List[I_OnRequest] = 1;
	}

	virtual ~ModuleHttpStats()
	{
		Srv->DelSocket(http);
	}

	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_STATIC|VF_VENDOR);
	}
};


class ModuleHttpStatsFactory : public ModuleFactory
{
 public:
	ModuleHttpStatsFactory()
	{
	}
	
	~ModuleHttpStatsFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleHttpStats(Me);
	}
};


extern "C" void * init_module( void )
{
	return new ModuleHttpStatsFactory;
}
