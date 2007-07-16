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
#include "users.h"
#include "commands/cmd_mode.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_mode(Instance);
}

/** Handle /MODE
 */
CmdResult cmd_mode::Handle (const char** parameters, int pcnt, userrec *user)
{
	ServerInstance->Modes->Process(parameters, pcnt, user, false);
	return CMD_SUCCESS;
}

