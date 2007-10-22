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
#include "commands/cmd_mode.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandMode(Instance);
}

/** Handle /MODE
 */
CmdResult CommandMode::Handle (const char** parameters, int pcnt, User *user)
{
	ServerInstance->Modes->Process(parameters, pcnt, user, false);
	return CMD_SUCCESS;
}

