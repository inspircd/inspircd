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
#include "commands/cmd_mode.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandMode(Instance);
}

/** Handle /MODE
 */
CmdResult CommandMode::Handle (const std::vector<std::string>& parameters, User *user)
{
	ServerInstance->Modes->Process(parameters, user, false);
	return CMD_SUCCESS;
}

