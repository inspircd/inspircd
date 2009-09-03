/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#ifndef __CMD_COMMANDS_H__
#define __CMD_COMMANDS_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /COMMANDS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandCommands : public Command
{
 public:
	/** Constructor for commands.
	 */
	CommandCommands (InspIRCd* Instance, Module* parent) : Command(Instance,parent,"COMMANDS",0,0) { }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


/** Handle /COMMANDS
 */
CmdResult CommandCommands::Handle (const std::vector<std::string>&, User *user)
{
	for (Commandtable::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
	{
		Module* src = i->second->creator;
		user->WriteNumeric(RPL_COMMANDS, "%s :%s %s %d %d",
				user->nick.c_str(),
				i->second->command.c_str(),
				src ? src->ModuleSourceFile.c_str() : "<core>",
				i->second->min_params,
				i->second->Penalty);
	}
	user->WriteNumeric(RPL_COMMANDSEND, "%s :End of COMMANDS list",user->nick.c_str());
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandCommands)
