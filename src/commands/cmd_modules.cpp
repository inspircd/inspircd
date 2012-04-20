/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "commands/cmd_modules.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandModules(Instance);
}

/** Handle /MODULES
 */
CmdResult CommandModules::Handle (const std::vector<std::string>&, User *user)
{
	std::vector<std::string> module_names = ServerInstance->Modules->GetAllModuleNames(0);

  	for (unsigned int i = 0; i < module_names.size(); i++)
	{
		Module* m = ServerInstance->Modules->Find(module_names[i]);
		Version V = m->GetVersion();

		if (user->HasPrivPermission("servers/auspex"))
		{
			std::string flags("Svsc");
			int pos = 0;
			for (int mult = 1; mult <= VF_SERVICEPROVIDER; mult *= 2, ++pos)
				if (!(V.Flags & mult))
					flags[pos] = '-';

			user->WriteNumeric(702, "%s :0x%08lx %s %s :%s", user->nick.c_str(), (unsigned long)m, module_names[i].c_str(), flags.c_str(), V.version.c_str());
		}
		else
		{
			user->WriteNumeric(702, "%s :%s",user->nick.c_str(), module_names[i].c_str());
		}
	}
	user->WriteNumeric(703, "%s :End of MODULES list",user->nick.c_str());

	return CMD_SUCCESS;
}
