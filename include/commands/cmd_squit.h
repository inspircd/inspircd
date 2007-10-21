/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *      the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __CMD_SQUIT_H__
#define __CMD_SQUIT_H__

// include the common header files

#include <string>
#include <vector>
#include "inspircd.h"
#include "users.h"
#include "channels.h"

/** Handle /SQUIT. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandSquit : public Command
{
 public:
	/** Constructor for squit.
	 */
	CommandSquit (InspIRCd* Instance) : Command(Instance,"SQUIT",'o',1) { syntax = "<servername> [<reason>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const char** parameters, int pcnt, User *user);
};

#endif
