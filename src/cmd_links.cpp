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
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands/cmd_links.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_links(Instance);
}

/** Handle /LINKS
 */
CmdResult cmd_links::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("364 %s %s %s :0 %s",user->nick,ServerInstance->Config->ServerName,ServerInstance->Config->ServerName,ServerInstance->Config->ServerDesc);
	user->WriteServ("365 %s * :End of /LINKS list.",user->nick);
	return CMD_SUCCESS;
}
