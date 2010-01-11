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

/** Handle /PING. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandPing : public Command
{
 public:
	/** Constructor for ping.
	 */
	CommandPing ( Module* parent) : Command(parent,"PING", 1, 2) { Penalty = 0; syntax = "<servername> [:<servername>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandPing::Handle (const std::vector<std::string>& parameters, User *user)
{
	user->WriteServ("PONG %s :%s", ServerInstance->Config->ServerName.c_str(), parameters[0].c_str());
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandPing)
