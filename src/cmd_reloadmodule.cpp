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
#include "commands/cmd_reloadmodule.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_reloadmodule(Instance);
}

CmdResult cmd_reloadmodule::Handle (const char** parameters, int pcnt, userrec *user)
{
	if (ServerInstance->UnloadModule(parameters[0]))
	{
		ServerInstance->WriteOpers("*** RELOAD MODULE: %s unloaded %s",user->nick, parameters[0]);
		if (ServerInstance->LoadModule(parameters[0]))
		{
			ServerInstance->WriteOpers("*** RELOAD MODULE: %s reloaded %s",user->nick, parameters[0]);
			user->WriteServ("975 %s %s :Module successfully reloaded.",user->nick, parameters[0]);
			return CMD_SUCCESS;
		}
	}
	
	ServerInstance->WriteOpers("*** RELOAD MODULE: %s unsuccessfully reloaded %s",user->nick, parameters[0]);
	user->WriteServ("975 %s %s :Module failed to reload.",user->nick, parameters[0]);
	return CMD_FAILURE;
}
