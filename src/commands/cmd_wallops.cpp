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

#ifndef __CMD_WALLOPS_H__
#define __CMD_WALLOPS_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /WALLOPS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandWallops : public Command
{
 public:
	/** Constructor for wallops.
	 */
	CommandWallops ( Module* parent) : Command(parent,"WALLOPS",1,1) { flags_needed = 'o'; syntax = "<any-text>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif




CmdResult CommandWallops::Handle (const std::vector<std::string>& parameters, User *user)
{
	user->WriteWallOps(std::string(parameters[0]));
	FOREACH_MOD(I_OnWallops,OnWallops(user,parameters[0]));
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandWallops)
