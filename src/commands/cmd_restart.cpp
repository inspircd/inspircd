/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_restart.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandRestart(Instance);
}

CmdResult CommandRestart::Handle (const char** parameters, int, User *user)
{
	ServerInstance->Log(DEFAULT,"Restart: %s",user->nick);
	if (!strcmp(parameters[0],ServerInstance->Config->restartpass))
	{
		ServerInstance->SNO->WriteToSnoMask('O', "RESTART command from %s!%s@%s, restarting server.",user->nick,user->ident,user->host);

		try
		{
			ServerInstance->Restart("Server restarting.");
		}
		catch (...)
		{
			/* We dont actually get here unless theres some fatal and unrecoverable error. */
			exit(0);
		}
	}
	else
	{
		ServerInstance->SNO->WriteToSnoMask('O', "Failed RESTART Command from %s!%s@%s.",user->nick,user->ident,user->host);
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

