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

#ifndef CMD_LINKS_H
#define CMD_LINKS_H

// include the common header files

#include "users.h"
#include "channels.h"

/** Handle /LINKS. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandLinks : public Command
{
 public:
	/** Constructor for links.
	 */
	CommandLinks ( Module* parent) : Command(parent,"LINKS",0,0) { }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif


/** Handle /LINKS
 */
CmdResult CommandLinks::Handle (const std::vector<std::string>&, User *user)
{
	user->WriteNumeric(364, "%s %s %s :0 %s",user->nick.c_str(),ServerInstance->Config->ServerName.c_str(),ServerInstance->Config->ServerName.c_str(),ServerInstance->Config->ServerDesc.c_str());
	user->WriteNumeric(365, "%s * :End of /LINKS list.",user->nick.c_str());
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandLinks)
