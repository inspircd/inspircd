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
#include "commands/cmd_server.h"



extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_server(Instance);
}

CmdResult cmd_server::Handle (const char** parameters, int pcnt, userrec *user)
{
	user->WriteServ("666 %s :You cannot identify as a server, you are a USER. IRC Operators informed.",user->nick);
	ServerInstance->WriteOpers("*** WARNING: %s attempted to issue a SERVER command and is registered as a user!",user->nick);
	return CMD_FAILURE;
}
