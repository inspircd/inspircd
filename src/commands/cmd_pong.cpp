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
#include "commands/cmd_pong.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandPong(Instance);
}

CmdResult CommandPong::Handle (const char**, int, User *user)
{
	// set the user as alive so they survive to next ping
	user->lastping = 1;
	return CMD_SUCCESS;
}
