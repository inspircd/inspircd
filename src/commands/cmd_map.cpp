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
#include "commands/cmd_map.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandMap(Instance);
}

/** Handle /MAP
 */
CmdResult CommandMap::Handle (const char**, int, User *user)
{
	// as with /LUSERS this does nothing without a linking
	// module to override its behaviour and display something
	// better.
	user->WriteServ("006 %s :%s",user->nick,ServerInstance->Config->ServerName);
	user->WriteServ("007 %s :End of /MAP",user->nick);

	return CMD_SUCCESS;
}
