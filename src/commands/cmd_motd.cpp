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
#include "commands/cmd_motd.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandMotd(Instance);
}

/** Handle /MOTD
 */
CmdResult CommandMotd::Handle (const char**, int, User *user)
{
	user->ShowMOTD();
	return CMD_SUCCESS;
}
