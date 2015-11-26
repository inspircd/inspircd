/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

class CommandReloadmodule : public Command
{
 public:
	/** Constructor for reloadmodule.
	 */
	CommandReloadmodule ( Module* parent) : Command( parent, "RELOADMODULE",1) { flags_needed = 'o'; syntax = "<modulename>"; }
	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

class ReloadAction : public HandlerBase0<void>
{
	Module* const mod;
	const std::string uuid;
	const std::string passedname;

 public:
	ReloadAction(Module* m, const std::string& uid, const std::string& passedmodname)
		: mod(m)
		, uuid(uid)
		, passedname(passedmodname)
	{
	}

	void Call()
	{
		DLLManager* dll = mod->ModuleDLLManager;
		std::string name = mod->ModuleSourceFile;
		ServerInstance->Modules->DoSafeUnload(mod);
		ServerInstance->GlobalCulls.Apply();
		delete dll;
		bool result = ServerInstance->Modules->Load(name);

		ServerInstance->SNO->WriteGlobalSno('a', "RELOAD MODULE: %s %ssuccessfully reloaded", passedname.c_str(), result ? "" : "un");
		User* user = ServerInstance->FindUUID(uuid);
		if (user)
			user->WriteNumeric(RPL_LOADEDMODULE, "%s :Module %ssuccessfully reloaded.", passedname.c_str(), result ? "" : "un");

		ServerInstance->GlobalCulls.AddItem(this);
	}
};

CmdResult CommandReloadmodule::Handle (const std::vector<std::string>& parameters, User *user)
{
	Module* m = ServerInstance->Modules->Find(parameters[0]);
	if (m == creator)
	{
		user->WriteNumeric(RPL_LOADEDMODULE, "%s :You cannot reload core_reloadmodule.so (unload and load it)",
			parameters[0].c_str());
		return CMD_FAILURE;
	}

	if (creator->dying)
		return CMD_FAILURE;

	if ((m) && (ServerInstance->Modules.CanUnload(m)))
	{
		ServerInstance->AtomicActions.AddAction(new ReloadAction(m, user->uuid, parameters[0]));
		return CMD_SUCCESS;
	}
	else
	{
		user->WriteNumeric(RPL_LOADEDMODULE, "%s :Could not find module by that name", parameters[0].c_str());
		return CMD_FAILURE;
	}
}

COMMAND_INIT(CommandReloadmodule)
