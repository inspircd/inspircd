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

/** Handle /PONG. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandPong : public Command
{
 public:
	/** Constructor for pong.
	 */
	CommandPong ( Module* parent) : Command(parent,"PONG", 0, 1) { Penalty = 0; syntax = "<ping-text>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

CmdResult CommandPong::Handle (const std::vector<std::string>&, User *user)
{
	// set the user as alive so they survive to next ping
	if (IS_LOCAL(user))
		IS_LOCAL(user)->lastping = 1;
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandPong)
