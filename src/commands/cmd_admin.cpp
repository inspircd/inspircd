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
#include "commands/cmd_admin.h"


extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandAdmin(Instance);
}

/** Handle /ADMIN
 */
CmdResult CommandAdmin::Handle (const char* const* parameters, int pcnt, User *user)
{
	user->WriteNumeric(256, "%s :Administrative info for %s",user->nick,ServerInstance->Config->ServerName);
	if (*ServerInstance->Config->AdminName)
		user->WriteNumeric(257, "%s :Name     - %s",user->nick,ServerInstance->Config->AdminName);
	user->WriteNumeric(258, "%s :Nickname - %s",user->nick,ServerInstance->Config->AdminNick);
	user->WriteNumeric(259, "%s :E-Mail   - %s",user->nick,ServerInstance->Config->AdminEmail);
	return CMD_SUCCESS;
}
