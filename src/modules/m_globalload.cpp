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
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName.c_str(), servername))
		{
			if (ServerInstance->Modules.Load(parameters[0].c_str()))
			{
				ServerInstance->SNO.WriteToSnoMask('a', "NEW MODULE '%s' GLOBALLY LOADED BY '%s'",parameters[0].c_str(), user->nick.c_str());
				user->WriteNumeric(RPL_LOADEDMODULE, parameters[0], "Module successfully loaded.");
			}
			else
			{
				user->WriteNumeric(ERR_CANTLOADMODULE, parameters[0], ServerInstance->Modules.LastError());
			}
		}
		else
			ServerInstance->SNO.WriteToSnoMask('a', "MODULE '%s' GLOBAL LOAD BY '%s' (not loaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
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

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (InspIRCd::Match(parameters[0], "core_*.so", ascii_case_insensitive_map))
		{
			user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0], "You cannot unload core commands!");
			return CMD_FAILURE;
		}

		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName.c_str(), servername))
		{
			Module* m = ServerInstance->Modules.Find(parameters[0]);
			if (m)
			{
				if (ServerInstance->Modules.Unload(m))
				{
					ServerInstance->SNO.WriteToSnoMask('a', "MODULE '%s' GLOBALLY UNLOADED BY '%s'",parameters[0].c_str(), user->nick.c_str());
					user->WriteRemoteNumeric(RPL_UNLOADEDMODULE, parameters[0], "Module successfully unloaded.");
				}
				else
				{
					user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0], ServerInstance->Modules.LastError());
				}
			}
			else
				user->WriteRemoteNumeric(ERR_CANTUNLOADMODULE, parameters[0], "No such module");
		}
		else
			ServerInstance->SNO.WriteToSnoMask('a', "MODULE '%s' GLOBAL UNLOAD BY '%s' (not unloaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_BROADCAST;
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

	CmdResult Handle(User* user, const Params& parameters) override
	{
		std::string servername = parameters.size() > 1 ? parameters[1] : "*";

		if (InspIRCd::Match(ServerInstance->Config->ServerName.c_str(), servername))
		{
			Module* m = ServerInstance->Modules.Find(parameters[0]);
			if (m)
			{
				ServerInstance->SNO.WriteToSnoMask('a', "MODULE '%s' GLOBALLY RELOADED BY '%s'", parameters[0].c_str(), user->nick.c_str());
				ServerInstance->Parser.CallHandler("RELOADMODULE", parameters, user);
			}
			else
			{
				user->WriteNumeric(RPL_LOADEDMODULE, parameters[0], "Could not find module by that name");
				return CMD_FAILURE;
			}
		}
		else
			ServerInstance->SNO.WriteToSnoMask('a', "MODULE '%s' GLOBAL RELOAD BY '%s' (not reloaded here)",parameters[0].c_str(), user->nick.c_str());

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
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

	Version GetVersion() override
	{
		return Version("Allows global loading of a module.", VF_COMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleGlobalLoad)
