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

/* $ModDesc: Provides support for the CHGHOST command */

/** Handle /CHGHOST
 */
class cmd_chghost : public command_t
{
 private:
	char*& hostmap;
 public:
	cmd_chghost (InspIRCd* Instance, char* &hmap) : command_t(Instance,"CHGHOST",'o',2), hostmap(hmap)
	{
		this->source = "m_chghost.so";
		syntax = "<nick> <newhost>";
	}
 
	CmdResult Handle(const char** parameters, int pcnt, userrec *user)
	{
		const char * x = parameters[1];

		for (; *x; x++)
		{
			if (!hostmap[*x])
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** Invalid characters in hostname");
				return CMD_FAILURE;
			}
		}
		if ((parameters[1] - x) > 63)
		{
			user->WriteServ("NOTICE %s :*** CHGHOST: Host too long",user->nick);
			return CMD_FAILURE;
		}
		userrec* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
		{
			if ((dest->ChangeDisplayedHost(parameters[1])) && (!ServerInstance->ULine(user->server)))
			{
				// fix by brain - ulines set hosts silently
				ServerInstance->WriteOpers(std::string(user->nick)+" used CHGHOST to make the displayed host of "+dest->nick+" become "+dest->dhost);
			}
			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
	}
};


class ModuleChgHost : public Module
{
	cmd_chghost* mycommand;
	char* hostmap;
 public:
	ModuleChgHost(InspIRCd* Me)
		: Module::Module(Me)
	{
		hostmap = new char[256];
		OnRehash("");
		mycommand = new cmd_chghost(ServerInstance, hostmap);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = 1;
	}
	
	void OnRehash(const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		std::string hmap = Conf.ReadValue("hostname", "charmap", 0);

		if (hmap.empty())
			hostmap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789";

		memset(&hostmap, 0, sizeof(hostmap));
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			hostmap[*n] = 1;
	}

	~ModuleChgHost()
	{
		delete[] hostmap;
	}
	
	Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
	
};


class ModuleChgHostFactory : public ModuleFactory
{
 public:
	ModuleChgHostFactory()
	{
	}
	
	~ModuleChgHostFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleChgHost(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleChgHostFactory;
}

