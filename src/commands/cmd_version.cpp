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
#include "commands/cmd_version.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandVersion(Instance);
}

CmdResult CommandVersion::Handle (const std::vector<std::string>&, User *user)
{
	user->WriteNumeric(RPL_VERSION, "%s :%s",user->nick.c_str(),ServerInstance->GetVersionString().c_str());
	ServerInstance->Config->Send005(user);
	return CMD_SUCCESS;
}
