/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
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
#include "helperfuncs.h"

/* $ModDesc: Gives /cban, aka C:lines. Think Q:lines, for channels. */

Server *Srv;

class cmd_cban : public command_t
{
	public:
	cmd_cban () : command_t("CBAN",'o',2)
	{
		this->source = "m_cban.so";
	}

	void Handle(char **parameters, int pcnt, userrec *user)
	{
		/* Handle CBAN here. */
	}
};

class ModuleCBan : public Module
{
	cmd_cban* mycommand;
	public:
		ModuleCBan(Server* Me) : Module::Module(Me)
		{
			Srv = Me;
			mycommand = new cmd_cban();
			Srv->AddCommand(mycommand);
		}

	virtual int OnUserPreJoin (userrec *user, chanrec *chan, const char *cname)
	{
		/* check cbans in here, and apply as necessary. */

		/* Allow the change. */
		return 0;
	}

	virtual ~ModuleCBan()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,0,VF_VENDOR);
	}
};


class ModuleCBanFactory : public ModuleFactory
{
 public:
	ModuleCBanFactory()
	{
	}
	
	~ModuleCBanFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleCBan(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCBanFactory;
}

