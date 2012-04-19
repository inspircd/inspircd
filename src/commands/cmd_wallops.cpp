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
#include "commands/cmd_wallops.h"



extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandWallops(Instance);
}

CmdResult CommandWallops::Handle (const std::vector<std::string>& parameters, User *user)
{
	user->WriteWallOps(std::string(parameters[0]));
	FOREACH_MOD(I_OnWallops,OnWallops(user,parameters[0]));
	return CMD_SUCCESS;
}
