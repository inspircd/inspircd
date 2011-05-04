/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/** Handle /SERVER. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandServer : public Command
{
 public:
	/** Constructor for server.
	 */
	CommandServer ( Module* parent) : Command(parent,"SERVER") { works_before_reg = true;}
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandServer::Handle (const std::vector<std::string>&, User *user)
{
	if (user->registered == REG_ALL)
	{
		user->WriteNumeric(ERR_ALREADYREGISTERED, "%s :You are already registered. (Perhaps your IRC client does not have a /SERVER command).",user->nick.c_str());
	}
	else
	{
		user->WriteNumeric(ERR_NOTREGISTERED, "%s :You may not register as a server (servers have seperate ports from clients, change your config)",name.c_str());
	}
	return CMD_FAILURE;
}

COMMAND_INIT(CommandServer)
