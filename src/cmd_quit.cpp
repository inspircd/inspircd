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
#include "modules.h"
#include "commands/cmd_quit.h"



extern "C" command_t* init_command(InspIRCd* Instance)
{
	return new cmd_quit(Instance);
}

CmdResult cmd_quit::Handle (const char** parameters, int pcnt, userrec *user)
{
	userrec::QuitUser(ServerInstance, user, pcnt ? parameters[0] : "Client exited");
	return CMD_SUCCESS;
}

