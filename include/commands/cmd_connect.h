/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *      the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CMD_CONNECT_H__
#define __CMD_CONNECT_H__

#include "users.h"
#include "channels.h"
#include "ctables.h"
#include "modules.h"

/** Handle /CONNECT. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandConnect : public Command
{
 public:
	/** Constructor for connect.
	 */
	CommandConnect (InspIRCd* Instance) : Command(Instance,"CONNECT","o",1,false,0) { syntax = "<servername> [<remote-server>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif
