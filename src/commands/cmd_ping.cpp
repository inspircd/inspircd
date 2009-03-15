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
#include "commands/cmd_ping.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandPing(Instance);
}

CmdResult CommandPing::Handle (const std::vector<std::string>& parameters, User *user)
{
	user->WriteServ("PONG %s :%s", ServerInstance->Config->ServerName, parameters[0].c_str());
	return CMD_SUCCESS;
}
