/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
