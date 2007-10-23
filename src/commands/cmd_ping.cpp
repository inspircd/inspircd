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
#include "commands/cmd_ping.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandPing(Instance);
}

CmdResult CommandPing::Handle (const char** parameters, int, User *user)
{
	user->WriteServ("PONG %s :%s",ServerInstance->Config->ServerName,parameters[0]);
	return CMD_SUCCESS;
}
