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
#include "commands/cmd_admin.h"


extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandAdmin(Instance);
}

/** Handle /ADMIN
 */
CmdResult CommandAdmin::Handle (const std::vector<std::string>& parameters, User *user)
{
	user->WriteNumeric(RPL_ADMINME, "%s :Administrative info for %s",user->nick.c_str(),ServerInstance->Config->ServerName);
	if (*ServerInstance->Config->AdminName)
		user->WriteNumeric(RPL_ADMINLOC1, "%s :Name     - %s",user->nick.c_str(),ServerInstance->Config->AdminName);
	user->WriteNumeric(RPL_ADMINLOC2, "%s :Nickname - %s",user->nick.c_str(),ServerInstance->Config->AdminNick);
	user->WriteNumeric(RPL_ADMINEMAIL, "%s :E-Mail   - %s",user->nick.c_str(),ServerInstance->Config->AdminEmail);
	return CMD_SUCCESS;
}
