/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

class CommandLoadmodule final
	: public Command
{
public:
	CommandLoadmodule(Module* parent)
		: Command(parent, "LOADMODULE", 1, 1)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<modulename>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override;
};

CmdResult CommandLoadmodule::Handle(User* user, const Params& parameters)
{
	if (ServerInstance->Modules.Load(parameters[0]))
	{
		ServerInstance->SNO.WriteGlobalSno('a', "NEW MODULE: {} loaded {}", user->nick, parameters[0]);
		user->WriteNumeric(RPL_LOADEDMODULE, parameters[0], "Module successfully loaded.");
		return CmdResult::SUCCESS;
	}
	else
	{
		user->WriteNumeric(ERR_CANTLOADMODULE, parameters[0], ServerInstance->Modules.LastError());
		return CmdResult::FAILURE;
	}
}

class CommandUnloadmodule final
	: public Command
{
public:
	CommandUnloadmodule(Module* parent)
		: Command(parent, "UNLOADMODULE", 1)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<modulename>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override;
};

CmdResult CommandUnloadmodule::Handle(User* user, const Params& parameters)
{
	if (InspIRCd::Match(parameters[0], "core_*", ascii_case_insensitive_map))
	{
		user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0], "You cannot unload core commands!");
		return CmdResult::FAILURE;
	}

	Module* m = ServerInstance->Modules.Find(parameters[0]);
	if (m == creator)
	{
		user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0], "You cannot unload module loading commands!");
		return CmdResult::FAILURE;
	}

	if (m && ServerInstance->Modules.Unload(m))
	{
		ServerInstance->SNO.WriteGlobalSno('a', "MODULE UNLOADED: {} unloaded {}", user->nick, parameters[0]);
		user->WriteNumeric(RPL_UNLOADEDMODULE, parameters[0], "Module successfully unloaded.");
	}
	else
	{
		user->WriteNumeric(ERR_CANTUNLOADMODULE, parameters[0], (m ? ServerInstance->Modules.LastError() : "No such module"));
		return CmdResult::FAILURE;
	}

	return CmdResult::SUCCESS;
}

class CoreModLoadModule final
	: public Module
{
	CommandLoadmodule cmdloadmod;
	CommandUnloadmodule cmdunloadmod;

public:
	CoreModLoadModule()
		: Module(VF_CORE | VF_VENDOR, "Provides the LOADMODULE and UNLOADMODULE commands")
		, cmdloadmod(this)
		, cmdunloadmod(this)
	{
	}
};

MODULE_INIT(CoreModLoadModule)
