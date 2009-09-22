/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides support for the CHGHOST command */

/** Handle /CHGHOST
 */
class CommandChghost : public Command
{
 private:
	char* hostmap;
 public:
	CommandChghost(Module* Creator, char* hmap) : Command(Creator,"CHGHOST", 2), hostmap(hmap)
	{
		flags_needed = 'o'; syntax = "<nick> <newhost>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		const char * x = parameters[1].c_str();

		for (; *x; x++)
		{
			if (!hostmap[(unsigned char)*x])
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** CHGHOST: Invalid characters in hostname");
				return CMD_FAILURE;
			}
		}
		if (parameters[1].empty())
		{
			user->WriteServ("NOTICE %s :*** CHGHOST: Host must be specified", user->nick.c_str());
			return CMD_FAILURE;
		}

		if ((parameters[1].c_str() - x) > 63)
		{
			user->WriteServ("NOTICE %s :*** CHGHOST: Host too long", user->nick.c_str());
			return CMD_FAILURE;
		}
		User* dest = ServerInstance->FindNick(parameters[0]);

		if (!dest)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (IS_LOCAL(dest))
		{
			if ((dest->ChangeDisplayedHost(parameters[1].c_str())) && (!ServerInstance->ULine(user->server)))
			{
				// fix by brain - ulines set hosts silently
				ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick)+" used CHGHOST to make the displayed host of "+dest->nick+" become "+dest->dhost);
			}
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};


class ModuleChgHost : public Module
{
	CommandChghost cmd;
	char hostmap[256];
 public:
	ModuleChgHost(InspIRCd* Me) : cmd(this, hostmap)
	{
		OnRehash(NULL);
		ServerInstance->AddCommand(&cmd);
		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}


	void OnRehash(User* user)
	{
		ConfigReader Conf(ServerInstance);
		std::string hmap = Conf.ReadValue("hostname", "charmap", 0);

		if (hmap.empty())
			hmap = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789";

		memset(hostmap, 0, sizeof(hostmap));
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			hostmap[(unsigned char)*n] = 1;
	}

	~ModuleChgHost()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for the CHGHOST command", VF_OPTCOMMON | VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleChgHost)
