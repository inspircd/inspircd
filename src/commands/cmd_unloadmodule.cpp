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
#include "commands/cmd_unloadmodule.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandUnloadmodule(Instance);
}

CmdResult CommandUnloadmodule::Handle (const char** parameters, int, User *user)
{
	if (ServerInstance->Modules->Unload(parameters[0]))
	{
		ServerInstance->WriteOpers("*** MODULE UNLOADED: %s unloaded %s", user->nick, parameters[0]);
		user->WriteServ("973 %s %s :Module successfully unloaded.",user->nick, parameters[0]);
	}
	else
	{
		user->WriteServ("972 %s %s :%s",user->nick, parameters[0], ServerInstance->Modules->LastError().c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}
