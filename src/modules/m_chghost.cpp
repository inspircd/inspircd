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

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for the CHGHOST command */

/** Handle /CHGHOST
 */
class cmd_chghost : public command_t
{
 private:
	char* hostmap;
 public:
	cmd_chghost (InspIRCd* Instance, char* hmap) : command_t(Instance,"CHGHOST",'o',2), hostmap(hmap)
	{
		this->source = "m_chghost.so";
		syntax = "<nick> <newhost>";
	}
 
	CmdResult Handle(const char** parameters, int pcnt, userrec *user)
	{
		const char * x = parameters[1];

		for (; *x; x++)
		{
			if (!hostmap[(unsigned char)*x])
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** CHGHOST: Invalid characters in hostname");
				return CMD_FAILURE;
			}
		}
		if (!*parameters[0])
		{
			user->WriteServ("NOTICE %s :*** CHGHOST: Host must be specified", user->nick);
			return CMD_FAILURE;
		}
		
		if ((parameters[1] - x) > 63)
		{
			user->WriteServ("NOTICE %s :*** CHGHOST: Host too long", user->nick);
			return CMD_FAILURE;
		}
		userrec* dest = ServerInstance->FindNick(parameters[0]);

		if (!dest)
		{
			user->WriteServ("401 %s %s :No such nick/channel", user->nick, parameters[0]);
			return CMD_FAILURE;
		}

		if ((dest->ChangeDisplayedHost(parameters[1])) && (!ServerInstance->ULine(user->server)))
		{
			// fix by brain - ulines set hosts silently
			ServerInstance->WriteOpers(std::string(user->nick)+" used CHGHOST to make the displayed host of "+dest->nick+" become "+dest->dhost);
		}

		/* route it! */
		return CMD_SUCCESS;

	}
};


class ModuleChgHost : public Module
{
	cmd_chghost* mycommand;
	char hostmap[256];
 public:
	ModuleChgHost(InspIRCd* Me)
		: Module(Me)
	{
		OnRehash(NULL,"");
		mycommand = new cmd_chghost(ServerInstance, hostmap);
		ServerInstance->AddCommand(mycommand);
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = 1;
	}
	
	void OnRehash(userrec* user, const std::string &parameter)
	{
		ConfigReader Conf(ServerInstance);
		std::string hmap = Conf.ReadValue("hostname", "charmap", 0);

		if (hmap.empty())
			hmap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789";

		memset(&hostmap, 0, 255);
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			hostmap[(unsigned char)*n] = 1;
	}

	~ModuleChgHost()
	{
	}
	
	Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
	
};

MODULE_INIT(ModuleChgHost)
