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

#ifndef __CMD_NICK_H__
#define __CMD_NICK_H__

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /NICK. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandNick : public Command
{
	bool allowinvalid;
 public:
	/** Constructor for nick.
	 */
	CommandNick (InspIRCd* Instance) : Command(Instance,"NICK", 0, 1, true, 3), allowinvalid(false) { syntax = "<newnick>"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const char** parameters, int pcnt, User *user);

	/** Handle internal command
	 * @param id Used to indicate if invalid nick changes are allowed.
	 * Set to 1 to allow invalid nicks and 0 to deny them.
	 * @param parameters Currently unused
	 */
	CmdResult HandleInternal(const unsigned int id, const std::deque<classbase*> &parameters);
};

#endif
