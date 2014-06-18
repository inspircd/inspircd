/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
		allow_empty_last_param = false;
		flags_needed = 'o';
		syntax = "<nick> <newhost>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		const char* x = parameters[1].c_str();

		if (parameters[1].length() > 63)
		{
			user->WriteServ("NOTICE %s :*** CHGHOST: Host too long", user->nick.c_str());
			return CMD_FAILURE;
		}

		for (; *x; x++)
		{
			if (!hostmap[(unsigned char)*x])
			{
				user->WriteServ("NOTICE "+user->nick+" :*** CHGHOST: Invalid characters in hostname");
				return CMD_FAILURE;
			}
		}

		User* dest = ServerInstance->FindNick(parameters[0]);

		// Allow services to change the host of unregistered users
		if ((!dest) || ((dest->registered != REG_ALL) && (!ServerInstance->ULine(user->server))))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (IS_LOCAL(dest))
		{
			if ((dest->ChangeDisplayedHost(parameters[1].c_str())) && (!ServerInstance->ULine(user->server)))
			{
				// fix by brain - ulines set hosts silently
				ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used CHGHOST to make the displayed host of "+dest->nick+" become "+dest->dhost);
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
	ModuleChgHost() : cmd(this, hostmap)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User* user)
	{
		std::string hmap = ServerInstance->Config->ConfValue("hostname")->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789");

		memset(hostmap, 0, sizeof(hostmap));
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			hostmap[(unsigned char)*n] = 1;
	}

	~ModuleChgHost()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for the CHGHOST command", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(ModuleChgHost)
