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
#include "configreader.h"
#include "modules.h"
#include "commands/cmd_wallops.h"



extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_wallops(Instance);
}

CmdResult cmd_wallops::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteWallOps(std::string(parameters[0]));
	FOREACH_MOD(I_OnWallops,OnWallops(user,parameters[0]));
	return CMD_SUCCESS;
}
