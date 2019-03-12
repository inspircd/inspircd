/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "core_info.h"

enum
{
	// From ircd-ratbox with an InspIRCd-specific format.
	RPL_MODLIST = 702,
	RPL_ENDOFMODLIST = 703
};

CommandModules::CommandModules(Module* parent)
	: ServerTargetCommand(parent, "MODULES")
{
	Penalty = 4;
	syntax = "[<servername>]";
}

/** Handle /MODULES
 */
CmdResult CommandModules::Handle(User* user, const Params& parameters)
{
	// Don't ask remote servers about their modules unless the local user asking is an oper
	// 2.0 asks anyway, so let's handle that the same way
	bool for_us = (parameters.empty() || irc::equals(parameters[0], ServerInstance->Config->ServerName));
	if ((!for_us) || (!IS_LOCAL(user)))
	{
		if (!user->IsOper())
		{
			user->WriteNotice("*** You cannot check what modules other servers have loaded.");
			return CMD_FAILURE;
		}

		// From an oper and not for us, forward
		if (!for_us)
			return CMD_SUCCESS;
	}

	const ModuleManager::ModuleMap& mods = ServerInstance->Modules->GetModules();

  	for (ModuleManager::ModuleMap::const_iterator i = mods.begin(); i != mods.end(); ++i)
	{
		Module* m = i->second;
		Version V = m->GetVersion();

		if (IS_LOCAL(user) && user->HasPrivPermission("servers/auspex"))
		{
			std::string flags("VCO");
			size_t pos = 0;
			for (int mult = 2; mult <= VF_OPTCOMMON; mult *= 2, ++pos)
				if (!(V.Flags & mult))
					flags[pos] = '-';

			std::string srcrev = m->ModuleDLLManager->GetVersion();
			user->WriteRemoteNumeric(RPL_MODLIST, m->ModuleSourceFile, srcrev.empty() ? "*" : srcrev, flags, V.description);
		}
		else
		{
			user->WriteRemoteNumeric(RPL_MODLIST, m->ModuleSourceFile, '*', '*', V.description);
		}
	}
	user->WriteRemoteNumeric(RPL_ENDOFMODLIST, "End of MODULES list");

	return CMD_SUCCESS;
}
