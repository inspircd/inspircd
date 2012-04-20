/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007-2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 John Brooks <john.brooks@dereferenced.net>
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


/* $ModDesc: Allows global loading of a module. */

#include "inspircd.h"

/** Handle /GLOADMODULE
 */
class CommandGloadmodule : public Command
{
 public:
	CommandGloadmodule (InspIRCd* Instance) : Command(Instance,"GLOADMODULE", "o", 1)
	{
		this->source = "m_globalload.so";
		syntax = "<modulename> [servermask]";
		TRANSLATE3(TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName, servername))
		{
			if (ServerInstance->Modules->Load(parameters[0].c_str()))
			{
				ServerInstance->SNO->WriteToSnoMask('a', "NEW MODULE '%s' GLOBALLY LOADED BY '%s'",parameters[0].c_str(), user->nick.c_str());
				user->WriteNumeric(975, "%s %s :Module successfully loaded.",user->nick.c_str(), parameters[0].c_str());
			}
			else
			{
				user->WriteNumeric(974, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
			}
		}
		else
			ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBAL LOAD BY '%s' (not loaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}
};

/** Handle /GUNLOADMODULE
 */
class CommandGunloadmodule : public Command
{
 public:
	CommandGunloadmodule (InspIRCd* Instance) : Command(Instance,"GUNLOADMODULE", "o", 1)
	{
		this->source = "m_globalload.so";
		syntax = "<modulename> [servermask]";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName, servername))
		{
			if (ServerInstance->Modules->Unload(parameters[0].c_str()))
			{
				ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBALLY UNLOADED BY '%s'",parameters[0].c_str(), user->nick.c_str());
				user->WriteNumeric(973, "%s %s :Module successfully unloaded.",user->nick.c_str(), parameters[0].c_str());
			}
			else
			{
				user->WriteNumeric(972, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
			}
		}
		else
			ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBAL UNLOAD BY '%s' (not unloaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}
};

/** Handle /GRELOADMODULE
 */
class CommandGreloadmodule : public Command
{
 public:
	CommandGreloadmodule (InspIRCd* Instance) : Command(Instance, "GRELOADMODULE", "o", 1)
	{
		this->source = "m_globalload.so";
		syntax = "<modulename> [servermask]";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName, servername))
		{
			bool ok = true;
			if (!ServerInstance->Modules->Unload(parameters[0].c_str()))
			{
				ok = false;
				user->WriteNumeric(972, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
			}
			if (!ServerInstance->Modules->Load(parameters[0].c_str()))
			{
				ok = false;
				user->WriteNumeric(974, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
			}
			ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBALLY RELOADED BY '%s'",parameters[0].c_str(), user->nick.c_str());
			if (ok)
				user->WriteNumeric(975, "%s %s :Module successfully loaded.",user->nick.c_str(), parameters[0].c_str());
		}
		else
			ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBAL RELOAD BY '%s' (not reloaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}
};

class ModuleGlobalLoad : public Module
{
	CommandGloadmodule *mycommand;
	CommandGunloadmodule *mycommand2;
	CommandGreloadmodule *mycommand3;

 public:
	ModuleGlobalLoad(InspIRCd* Me) : Module(Me)
	{

		mycommand = new CommandGloadmodule(ServerInstance);
		mycommand2 = new CommandGunloadmodule(ServerInstance);
		mycommand3 = new CommandGreloadmodule(ServerInstance);
		ServerInstance->AddCommand(mycommand);
		ServerInstance->AddCommand(mycommand2);
		ServerInstance->AddCommand(mycommand3);

	}

	virtual ~ModuleGlobalLoad()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleGlobalLoad)

