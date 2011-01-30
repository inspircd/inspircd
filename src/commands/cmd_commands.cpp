/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "command_parse.h"

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
	CommandCommands ( Module* parent) : Command(parent,"COMMANDS",0,0) { }
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
	std::vector<std::string> list;
	list.reserve(ServerInstance->Parser->cmdlist.size());
	for (Commandtable::iterator i = ServerInstance->Parser->cmdlist.begin(); i != ServerInstance->Parser->cmdlist.end(); i++)
	{
		Module* src = i->second->creator;
		char buffer[MAXBUF];
		snprintf(buffer, MAXBUF, ":%s %03d %s :%s %s %d %d",
			ServerInstance->Config->ServerName.c_str(), RPL_COMMANDS, user->nick.c_str(),
			i->second->name.c_str(), src->ModuleSourceFile.c_str(),
			i->second->min_params, i->second->Penalty);
		list.push_back(buffer);
	}
	sort(list.begin(), list.end());
	for(unsigned int i=0; i < list.size(); i++)
		user->Write(list[i]);
	user->WriteNumeric(RPL_COMMANDSEND, "%s :End of COMMANDS list",user->nick.c_str());
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandCommands)
