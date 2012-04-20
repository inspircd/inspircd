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
	CommandSethost (InspIRCd* Instance, char* hmap) : Command(Instance,"SETHOST","o",1), hostmap(hmap)
	{
		this->source = "m_sethost.so";
		syntax = "<new-hostname>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		size_t len = 0;
		for (std::string::const_iterator x = parameters[0].begin(); x != parameters[0].end(); x++, len++)
		{
			if (!hostmap[(const unsigned char)*x])
			{
				user->WriteServ("NOTICE "+std::string(user->nick)+" :*** SETHOST: Invalid characters in hostname");
				return CMD_FAILURE;
			}
		}
		if (len == 0)
		{
			user->WriteServ("NOTICE %s :*** SETHOST: Host must be specified", user->nick.c_str());
			return CMD_FAILURE;
		}
		if (len > 64)
		{
			user->WriteServ("NOTICE %s :*** SETHOST: Host too long",user->nick.c_str());
			return CMD_FAILURE;
		}

		if (user->ChangeDisplayedHost(parameters[0].c_str()))
		{
			ServerInstance->SNO->WriteGlobalSno('a', std::string(user->nick)+" used SETHOST to change their displayed host to "+user->dhost);
			return CMD_LOCALONLY;
		}

		return CMD_FAILURE;
	}
};


class ModuleSetHost : public Module
{
	CommandSethost* mycommand;
	char hostmap[256];
 public:
	ModuleSetHost(InspIRCd* Me)
		: Module(Me)
	{
		OnRehash(NULL);
		mycommand = new CommandSethost(ServerInstance, hostmap);
		ServerInstance->AddCommand(mycommand);
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

	virtual ~ModuleSetHost()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

};

MODULE_INIT(ModuleSetHost)
