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
#include "commands/cmd_version.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandVersion(Instance);
}

CmdResult CommandVersion::Handle (const char**, int, User *user)
{
	user->WriteServ("351 %s :%s",user->nick,ServerInstance->GetVersionString().c_str());
	ServerInstance->Config->Send005(user);
	return CMD_SUCCESS;
}
