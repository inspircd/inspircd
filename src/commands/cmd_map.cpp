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
#include "commands/cmd_map.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandMap(Instance);
}

/** Handle /MAP
 */
CmdResult CommandMap::Handle (const std::vector<std::string>&, User *user)
{
	// as with /LUSERS this does nothing without a linking
	// module to override its behaviour and display something
	// better.
	user->WriteNumeric(006, "%s :%s",user->nick.c_str(),ServerInstance->Config->ServerName);
	user->WriteNumeric(007, "%s :End of /MAP",user->nick.c_str());

	return CMD_SUCCESS;
}
