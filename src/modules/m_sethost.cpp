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

/* $ModDesc: Provides support for the SETHOST command */

/** Handle /SETHOST
 */
class CommandSethost : public Command
{
 private:
	char* hostmap;
 public:
	CommandSethost(Module* Creator, char* hmap) : Command(Creator,"SETHOST", 1), hostmap(hmap)
	{
		allow_empty_last_param = false;
		flags_needed = 'o'; syntax = "<new-hostname>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		size_t len = 0;
		for (std::string::const_iterator x = parameters[0].begin(); x != parameters[0].end(); x++, len++)
		{
			if (!hostmap[(const unsigned char)*x])
			{
				user->WriteServ("NOTICE "+user->nick+" :*** SETHOST: Invalid characters in hostname");
				return CMD_FAILURE;
			}
		}

		if (len > 64)
		{
			user->WriteServ("NOTICE %s :*** SETHOST: Host too long",user->nick.c_str());
			return CMD_FAILURE;
		}

		if (user->ChangeDisplayedHost(parameters[0].c_str()))
		{
			ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used SETHOST to change their displayed host to "+user->dhost);
			return CMD_SUCCESS;
		}

		return CMD_FAILURE;
	}
};


class ModuleSetHost : public Module
{
	CommandSethost cmd;
	char hostmap[256];
 public:
	ModuleSetHost()
		: cmd(this, hostmap)
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

	virtual ~ModuleSetHost()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for the SETHOST command", VF_VENDOR);
	}

};

MODULE_INIT(ModuleSetHost)
