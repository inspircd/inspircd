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
#include "commands/cmd_commands.h"

/** Handle /COMMANDS
 */
extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandCommands(Instance);
}

CmdResult CommandCommands::Handle (const std::vector<std::string>&, User *user)
{
	for (Commandtable::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
	{
		user->WriteNumeric(RPL_COMMANDS, "%s :%s %s %d %d",
				user->nick.c_str(),
				i->second->command.c_str(),
				i->second->source.c_str(),
				i->second->min_params,
				i->second->Penalty);
	}
	user->WriteNumeric(RPL_COMMANDSEND, "%s :End of COMMANDS list",user->nick.c_str());
	return CMD_SUCCESS;
}
