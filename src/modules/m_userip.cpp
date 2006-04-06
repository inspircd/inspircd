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
#include <string>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "helperfuncs.h"

/* $ModDesc: Provides support for USERIP command */

static Server *Srv;

class cmd_userip : public command_t
{
 public:
	cmd_userip () : command_t("USERIP", 'o', 1)
	{
		this->source = "m_userip.so";
	}

	void Handle (char **parameters, int pcnt, userrec *user)
	{
		char Return[MAXBUF],junk[MAXBUF];
		snprintf(Return,MAXBUF,"340 %s :",user->nick);
		for (int i = 0; i < pcnt; i++)
		{
			userrec *u = Find(parameters[i]);
			if (u)
			{
				snprintf(junk,MAXBUF,"%s%s=+%s@%s ",u->nick,*u->oper ? "*" : "",u->ident,(char*)inet_ntoa(u->ip4));
				strlcat(Return,junk,MAXBUF);
			}
		}
		WriteServ(user->fd,Return);
	}
};

class ModuleUserIP : public Module
{
	cmd_userip* mycommand;
 public:
	ModuleUserIP(Server* Me)
		: Module::Module(Me)
	{
		Srv = Me;
		mycommand = new cmd_userip();
		Srv->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_On005Numeric] = 1;
	}

	virtual void On005Numeric(std::string &output)
	{
		output = output + std::string(" USERIP");
	}
	
	virtual ~ModuleUserIP()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,0,0,1,VF_VENDOR);
	}
	
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleUserIPFactory : public ModuleFactory
{
 public:
	ModuleUserIPFactory()
	{
	}
	
	~ModuleUserIPFactory()
	{
	}
	
	virtual Module * CreateModule(Server* Me)
	{
		return new ModuleUserIP(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleUserIPFactory;
}

