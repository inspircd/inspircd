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
#include "commands/cmd_admin.h"


extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_admin(Instance);
}

/** Handle /ADMIN
 */
CmdResult cmd_admin::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("256 %s :Administrative info for %s",user->nick,ServerInstance->Config->ServerName);
	if (*ServerInstance->Config->AdminName)
		user->WriteServ("257 %s :Name     - %s",user->nick,ServerInstance->Config->AdminName);
	user->WriteServ("258 %s :Nickname - %s",user->nick,ServerInstance->Config->AdminNick);
	user->WriteServ("258 %s :E-Mail   - %s",user->nick,ServerInstance->Config->AdminEmail);
	return CMD_SUCCESS;
}
