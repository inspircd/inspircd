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
#include "commands/cmd_pong.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_pong(Instance);
}

CmdResult cmd_pong::Handle (const char** parameters, int pcnt, userrec *user)
{
	// set the user as alive so they survive to next ping
	user->lastping = 1;
	return CMD_SUCCESS;
}
