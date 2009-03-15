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
#include "commands/cmd_rules.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandRules(Instance);
}

CmdResult CommandRules::Handle (const std::vector<std::string>& parameters, User *user)
{
	user->ShowRULES();
	return CMD_SUCCESS;
}
