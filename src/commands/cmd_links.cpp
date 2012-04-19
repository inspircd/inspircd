/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "commands/cmd_links.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandLinks(Instance);
}

/** Handle /LINKS
 */
CmdResult CommandLinks::Handle (const std::vector<std::string>&, User *user)
{
	user->WriteNumeric(364, "%s %s %s :0 %s",user->nick.c_str(),ServerInstance->Config->ServerName,ServerInstance->Config->ServerName,ServerInstance->Config->ServerDesc);
	user->WriteNumeric(365, "%s * :End of /LINKS list.",user->nick.c_str());
	return CMD_SUCCESS;
}
