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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "core_info.h"

CommandModules::CommandModules(Module* parent)
	: Command(parent, "MODULES", 0, 0)
{
	Penalty = 4;
	syntax = "[<servername>]";
}

/** Handle /MODULES
 */
CmdResult CommandModules::Handle (const std::vector<std::string>& parameters, User *user)
{
	// Don't ask remote servers about their modules unless the local user asking is an oper
	// 2.0 asks anyway, so let's handle that the same way
	bool for_us = (parameters.empty() || parameters[0] == ServerInstance->Config->ServerName);
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
			std::string flags("vcC");
			int pos = 0;
			for (int mult = 2; mult <= VF_OPTCOMMON; mult *= 2, ++pos)
				if (!(V.Flags & mult))
					flags[pos] = '-';

#ifdef PURE_STATIC
			user->SendText(":%s 702 %s :%s %s :%s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), m->ModuleSourceFile.c_str(), flags.c_str(), V.description.c_str());
#else
			std::string srcrev = m->ModuleDLLManager->GetVersion();
			user->SendText(":%s 702 %s :%s %s :%s - %s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), m->ModuleSourceFile.c_str(), flags.c_str(), V.description.c_str(), srcrev.c_str());
#endif
		}
		else
		{
			user->SendText(":%s 702 %s :%s %s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), m->ModuleSourceFile.c_str(), V.description.c_str());
		}
	}
	user->SendText(":%s 703 %s :End of MODULES list", ServerInstance->Config->ServerName.c_str(), user->nick.c_str());

	return CMD_SUCCESS;
}

RouteDescriptor CommandModules::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	if (parameters.size() >= 1)
		return ROUTE_UNICAST(parameters[0]);
	return ROUTE_LOCALONLY;
}
