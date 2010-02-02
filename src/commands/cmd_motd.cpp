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

/** Handle /MOTD. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandMotd : public Command
{
 public:
	/** Constructor for motd.
	 */
	CommandMotd ( Module* parent) : Command(parent,"MOTD",0,1) { syntax = "[<servername>]"; }
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

/** Handle /MOTD
 */
CmdResult CommandMotd::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 0 && parameters[0] != ServerInstance->Config->ServerName)
		return CMD_SUCCESS;

	ConfigFileCache::iterator motd = ServerInstance->Config->Files.find("motd");
	if (motd == ServerInstance->Config->Files.end())
	{
		user->SendText(":%s %03d %s :Message of the day file is missing.",
			ServerInstance->Config->ServerName.c_str(), ERR_NOMOTD, user->nick.c_str());
		return CMD_SUCCESS;
	}

	user->SendText(":%s %03d %s :%s message of the day", ServerInstance->Config->ServerName.c_str(),
		RPL_MOTDSTART, user->nick.c_str(), ServerInstance->Config->ServerName.c_str());

	for (file_cache::iterator i = motd->second.begin(); i != motd->second.end(); i++)
		user->SendText(":%s %03d %s :- %s", ServerInstance->Config->ServerName.c_str(), RPL_MOTD, user->nick.c_str(),i->c_str());

	user->SendText(":%s %03d %s :End of message of the day.", ServerInstance->Config->ServerName.c_str(), RPL_ENDOFMOTD, user->nick.c_str());

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandMotd)
