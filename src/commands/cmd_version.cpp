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

#ifndef CMD_VERSION_H
#define CMD_VERSION_H

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /VERSION. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandVersion : public Command
{
 public:
	/** Constructor for version.
	 */
	CommandVersion ( Module* parent) : Command(parent,"VERSION",0,0) { syntax = "[<servername>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif




CmdResult CommandVersion::Handle (const std::vector<std::string>&, User *user)
{
	user->WriteNumeric(RPL_VERSION, "%s :%s",user->nick.c_str(),ServerInstance->GetVersionString(IS_OPER(user)).c_str());
	ServerInstance->Config->Send005(user);
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandVersion)
