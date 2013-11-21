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

/** Handle /MODULES. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandModules : public Command
{
 public:
	/** Constructor for modules.
	 */
	CommandModules ( Module* parent) : Command(parent,"MODULES",0,0) { syntax = "[server]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() >= 1)
			return ROUTE_UNICAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}
};

/** Handle /MODULES
 */
CmdResult CommandModules::Handle (const std::vector<std::string>& parameters, User *user)
{
	//	Don't ask remote servers about their modules unless the local user asking is an oper
	//	2.0 asks anyway, so let's handle that the same way
	if ((parameters.size() >= 1 && parameters[0] != ServerInstance->Config->ServerName && !user->IsOper())
		|| (!IS_LOCAL(user) && !user->IsOper()))
	{
		user->WriteNotice("*** You cannot check what modules other servers have loaded.");
		return CMD_FAILURE;
	}


	const ModuleManager::ModuleMap& mods = ServerInstance->Modules->GetModules();

  	for (ModuleManager::ModuleMap::const_iterator i = mods.begin(); i != mods.end(); ++i)
	{
		Module* m = i->second;
		Version V = m->GetVersion();

		if (user->HasPrivPermission("servers/auspex"))
		{
			std::string flags("SvcC");
			int pos = 0;
			for (int mult = 1; mult <= VF_OPTCOMMON; mult *= 2, ++pos)
				if (!(V.Flags & mult))
					flags[pos] = '-';

#ifdef PURE_STATIC
			user->SendText(":%s 702 %s :%p %s %s :%s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), (void*)m, m->ModuleSourceFile.c_str(), flags.c_str(), V.description.c_str());
#else
			std::string srcrev = m->ModuleDLLManager->GetVersion();
			user->SendText(":%s 702 %s :%p %s %s :%s - %s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), (void*)m, m->ModuleSourceFile.c_str(), flags.c_str(), V.description.c_str(), srcrev.c_str());
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

COMMAND_INIT(CommandModules)
