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
	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		if (parameters.size() > 0)
			return ROUTE_UNICAST(parameters[0]);
		return ROUTE_LOCALONLY;
	}
};

/** Handle /ADMIN
 */
CmdResult CommandAdmin::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 0 && parameters[0] != ServerInstance->Config->ServerName)
		return CMD_SUCCESS;
	user->SendText(":%s %03d %s :Administrative info for %s", ServerInstance->Config->ServerName.c_str(),
		RPL_ADMINME, user->nick.c_str(),ServerInstance->Config->ServerName.c_str());
	if (!ServerInstance->Config->AdminName.empty())
		user->SendText(":%s %03d %s :Name     - %s", ServerInstance->Config->ServerName.c_str(),
			RPL_ADMINLOC1, user->nick.c_str(), ServerInstance->Config->AdminName.c_str());
	user->SendText(":%s %03d %s :Nickname - %s", ServerInstance->Config->ServerName.c_str(),
		RPL_ADMINLOC2, user->nick.c_str(), ServerInstance->Config->AdminNick.c_str());
	user->SendText(":%s %03d %s :E-Mail   - %s", ServerInstance->Config->ServerName.c_str(),
		RPL_ADMINEMAIL, user->nick.c_str(), ServerInstance->Config->AdminEmail.c_str());
	return CMD_SUCCESS;
}

COMMAND_INIT(CommandAdmin)
