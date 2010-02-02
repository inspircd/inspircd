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

/** Handle /RULES. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandRules : public Command
{
 public:
	/** Constructor for rules.
	 */
	CommandRules ( Module* parent) : Command(parent,"RULES",0,0) { syntax = "[<servername>]"; }
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

CmdResult CommandRules::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() > 0 && parameters[0] != ServerInstance->Config->ServerName)
		return CMD_SUCCESS;

	ConfigFileCache::iterator rules = ServerInstance->Config->Files.find("rules");
	if (rules == ServerInstance->Config->Files.end())
	{
		user->SendText(":%s %03d %s :RULES file is missing.",
			ServerInstance->Config->ServerName.c_str(), ERR_NORULES, user->nick.c_str());
		return CMD_SUCCESS;
	}
	user->SendText(":%s %03d %s :%s server rules:", ServerInstance->Config->ServerName.c_str(),
		RPL_RULESTART, user->nick.c_str(), ServerInstance->Config->ServerName.c_str());

	for (file_cache::iterator i = rules->second.begin(); i != rules->second.end(); i++)
		user->SendText(":%s %03d %s :- %s", ServerInstance->Config->ServerName.c_str(), RPL_RULES, user->nick.c_str(),i->c_str());

	user->SendText(":%s %03d %s :End of RULES command.", ServerInstance->Config->ServerName.c_str(), RPL_RULESEND, user->nick.c_str());

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandRules)
