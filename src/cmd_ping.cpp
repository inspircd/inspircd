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
#include "commands/cmd_ping.h"

extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_ping(Instance);
}

CmdResult cmd_ping::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("PONG %s :%s",ServerInstance->Config->ServerName,parameters[0]);
	return CMD_SUCCESS;
}
