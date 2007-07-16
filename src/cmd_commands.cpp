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
#include "commands/cmd_commands.h"

/** Handle /COMMANDS
 */
extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_commands(Instance);
}

CmdResult cmd_commands::Handle (const char** parameters, int pcnt, userrec *user)
{
	for (command_table::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
	{
		user->WriteServ("902 %s :%s %s %d",user->nick,i->second->command.c_str(),i->second->source.c_str(),i->second->min_params);
	}
	user->WriteServ("903 %s :End of COMMANDS list",user->nick);
	return CMD_SUCCESS;
}
