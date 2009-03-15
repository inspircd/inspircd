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
#include "commands/cmd_motd.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandMotd(Instance);
}

/** Handle /MOTD
 */
CmdResult CommandMotd::Handle (const std::vector<std::string>&, User *user)
{
	user->ShowMOTD();
	return CMD_SUCCESS;
}
