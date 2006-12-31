/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
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

#include "inspircd.h"

/* $ModDesc: Provides support for the SETHOST command */

/** Handle /SETHOST
 */
class cmd_sethost : public command_t
{
 private:
	char*& hostmap;
 public:
	cmd_sethost (InspIRCd* Instance, char*& hmap) : command_t(Instance,"SETHOST",'o',1), hostmap(hmap)
	{
		this->source = "m_sethost.so";
		syntax = "<new-hostname>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		size_t len = 0;
		for (const char* x = parameters[0]; *x; x++, len++)
		{
			if (!strchr(hostmap, *x))
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in hostname");
				return CMD_FAILURE;
			}
		}
		if (len > 64)
		{
			user->WriteServ("NOTICE %s :*** SETHOST: Host too long",user->nick);
			return CMD_FAILURE;
		}
		if (user->ChangeDisplayedHost(parameters[0]))
		{
			ServerInstance->WriteOpers(std::string(user->nick)+" used SETHOST to change their displayed host to "+user->dhost);
			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
	}
};


class ModuleSetHost : public Module
{
	cmd_sethost* mycommand;
	char* hostmap;
	std::string hmap;
 public:
	ModuleSetHost(InspIRCd* Me)
		: Module::Module(Me)
	{	
		OnRehash("");
		mycommand = new cmd_sethost(ServerInstance, hostmap);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = 1;
	}

	void OnRehash(const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		hmap = Conf.ReadValue("hostname", "charmap", 0);

		if (hmap.empty())
			hostmap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789";
		else
			hostmap = (char*)hmap.c_str();
	}

	virtual ~ModuleSetHost()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
	
};

class ModuleSetHostFactory : public ModuleFactory
{
 public:
	ModuleSetHostFactory()
	{
	}
	
	~ModuleSetHostFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleSetHost(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleSetHostFactory;
}

