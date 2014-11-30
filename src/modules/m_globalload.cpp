/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
	CommandGloadmodule(Module* Creator) : Command(Creator,"GLOADMODULE", 1)
	{
		flags_needed = 'o';
		syntax = "<modulename> [servermask]";
		TRANSLATE3(TR_TEXT, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName.c_str(), servername))
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

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

/** Handle /GUNLOADMODULE
 */
class CommandGunloadmodule : public Command
{
 public:
	CommandGunloadmodule(Module* Creator) : Command(Creator,"GUNLOADMODULE", 1)
	{
		flags_needed = 'o';
		syntax = "<modulename> [servermask]";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName.c_str(), servername))
		{
			Module* m = ServerInstance->Modules->Find(parameters[0]);
			if (m)
			{
				if (ServerInstance->Modules->Unload(m))
				{
					ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBALLY UNLOADED BY '%s'",parameters[0].c_str(), user->nick.c_str());
					user->SendText(":%s 973 %s %s :Module successfully unloaded.",
						ServerInstance->Config->ServerName.c_str(), user->nick.c_str(), parameters[0].c_str());
				}
				else
				{
					user->WriteNumeric(972, "%s %s :%s",user->nick.c_str(), parameters[0].c_str(), ServerInstance->Modules->LastError().c_str());
				}
			}
			else
				user->SendText(":%s 972 %s %s :No such module", ServerInstance->Config->ServerName.c_str(), user->nick.c_str(), parameters[0].c_str());
		}
		else
			ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBAL UNLOAD BY '%s' (not unloaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class GReloadModuleWorker : public HandlerBase1<void, bool>
{
 public:
	const std::string nick;
	const std::string name;
	const std::string uid;
	GReloadModuleWorker(const std::string& usernick, const std::string& uuid, const std::string& modn)
		: nick(usernick), name(modn), uid(uuid) {}
	void Call(bool result)
	{
		ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBALLY RELOADED BY '%s'%s", name.c_str(), nick.c_str(), result ? "" : " (failed here)");
		User* user = ServerInstance->FindNick(uid);
		if (user)
			user->WriteNumeric(975, "%s %s :Module %ssuccessfully reloaded.",
				user->nick.c_str(), name.c_str(), result ? "" : "un");
		ServerInstance->GlobalCulls.AddItem(this);
	}
};

/** Handle /GRELOADMODULE
 */
class CommandGreloadmodule : public Command
{
 public:
	CommandGreloadmodule(Module* Creator) : Command(Creator, "GRELOADMODULE", 1)
	{
		flags_needed = 'o'; syntax = "<modulename> [servermask]";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName.c_str(), servername))
		{
			Module* m = ServerInstance->Modules->Find(parameters[0]);
			if (m)
			{
				GReloadModuleWorker* worker = NULL;
				if ((m != creator) && (!creator->dying))
					worker = new GReloadModuleWorker(user->nick, user->uuid, parameters[0]);
				ServerInstance->Modules->Reload(m, worker);
			}
			else
			{
				user->WriteNumeric(975, "%s %s :Could not find module by that name", user->nick.c_str(), parameters[0].c_str());
				return CMD_FAILURE;
			}
		}
		else
			ServerInstance->SNO->WriteToSnoMask('a', "MODULE '%s' GLOBAL RELOAD BY '%s' (not reloaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleGlobalLoad : public Module
{
	CommandGloadmodule cmd1;
	CommandGunloadmodule cmd2;
	CommandGreloadmodule cmd3;

 public:
	ModuleGlobalLoad()
		: cmd1(this), cmd2(this), cmd3(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd1);
		ServerInstance->Modules->AddService(cmd2);
		ServerInstance->Modules->AddService(cmd3);
	}

	~ModuleGlobalLoad()
	{
	}

	Version GetVersion()
	{
		return Version("Allows global loading of a module.", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleGlobalLoad)

