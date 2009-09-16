/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#ifndef __CMD_MODULES_H__
#define __CMD_MODULES_H__

// include the common header files

#include "users.h"
#include "channels.h"

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
	CommandModules ( Module* parent) : Command(parent,"MODULES",0,0) { syntax = "[debug]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


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

COMMAND_INIT(CommandModules)
