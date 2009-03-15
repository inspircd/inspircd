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
#include "commands/cmd_restart.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandRestart(Instance);
}

CmdResult CommandRestart::Handle (const std::vector<std::string>& parameters, User *user)
{
	ServerInstance->Logs->Log("COMMAND",DEFAULT,"Restart: %s",user->nick.c_str());
	if (!ServerInstance->PassCompare(user, ServerInstance->Config->restartpass, parameters[0].c_str(), ServerInstance->Config->powerhash))
	{
		ServerInstance->SNO->WriteToSnoMask('A', "RESTART command from %s!%s@%s, restarting server.", user->nick.c_str(), user->ident.c_str(), user->host.c_str());

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
		ServerInstance->SNO->WriteToSnoMask('A', "Failed RESTART Command from %s!%s@%s.", user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}

