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
#include "commands/cmd_commands.h"

/** Handle /COMMANDS
 */
extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandCommands(Instance);
}

CmdResult CommandCommands::Handle (const char**, int, User *user)
{
	for (Commandable::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
	{
		user->WriteServ("902 %s :%s %s %d %d",
				user->nick,
				i->second->command.c_str(),
				i->second->source.c_str(),
				i->second->min_params,
				i->second->Penalty);
	}
	user->WriteServ("903 %s :End of COMMANDS list",user->nick);
	return CMD_SUCCESS;
}
