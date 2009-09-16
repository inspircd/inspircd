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

#ifndef __CMD_ADMIN_H__
#define __CMD_ADMIN_H__

#include "users.h"
#include "channels.h"

/** Handle /ADMIN. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandAdmin : public Command
{
 public:
	/** Constructor for admin.
	 */
	CommandAdmin(Module* parent) : Command(parent,"ADMIN",0,0) { syntax = "[<servername>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

#endif



/** Handle /ADMIN
 */
CmdResult CommandAdmin::Handle (const std::vector<std::string>& parameters, User *user)
{
	user->WriteNumeric(RPL_ADMINME, "%s :Administrative info for %s",user->nick.c_str(),ServerInstance->Config->ServerName);
	if (*ServerInstance->Config->AdminName)
		user->WriteNumeric(RPL_ADMINLOC1, "%s :Name     - %s",user->nick.c_str(),ServerInstance->Config->AdminName);
	user->WriteNumeric(RPL_ADMINLOC2, "%s :Nickname - %s",user->nick.c_str(),ServerInstance->Config->AdminNick);
	user->WriteNumeric(RPL_ADMINEMAIL, "%s :E-Mail   - %s",user->nick.c_str(),ServerInstance->Config->AdminEmail);
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandAdmin)
