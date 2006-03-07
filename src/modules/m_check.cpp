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

/* $ModDesc: Provides the /check command to retrieve information on a user, channel, or IP address */

static Server *Srv;

class cmd_check : public command_t
{
 public:
	cmd_check() : command_t("CHECK", 'o', 1)
	{
		this->source = "m_check.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{

	}
};


class ModuleCheck : public Module
{
 private:
	cmd_check *mycommand;
 public:
	ModuleCheck(Server* Me) : Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_check();
		Srv->AddCommand(mycommand);
	}
	
	virtual ~ModuleCheck()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_VENDOR);
	}

	void Implements(char* List)
	{
		/* we don't hook anything, nothing required */
	}
	
};



class ModuleCheckFactory : public ModuleFactory
{
 public:
	ModuleCheckFactory()
	{
	}
	
	~ModuleCheckFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleCheck(Me);
	}
	
};

extern "C" void * init_module( void )
{
	return new ModuleCheckFactory;
}

