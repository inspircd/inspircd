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
#include "users.h"
#include "commands/cmd_summon.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_summon(Instance);
}

CmdResult cmd_summon::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("445 %s :SUMMON has been disabled (depreciated command)",user->nick);
	return CMD_FAILURE;
}
