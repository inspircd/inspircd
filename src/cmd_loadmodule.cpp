/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "users.h"
#include "commands/cmd_loadmodule.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_loadmodule(Instance);
}

/** Handle /LOADMODULE
 */
CmdResult cmd_loadmodule::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (ServerInstance->LoadModule(parameters[0]))
	{
		ServerInstance->WriteOpers("*** NEW MODULE: %s loaded %s",user->nick, parameters[0]);
		user->WriteServ("975 %s %s :Module successfully loaded.",user->nick, parameters[0]);
		return CMD_SUCCESS;
	}
	else
	{
		user->WriteServ("974 %s %s :Failed to load module: %s",user->nick, parameters[0],ServerInstance->ModuleError());
		return CMD_FAILURE;
	}
}

