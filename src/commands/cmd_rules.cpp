/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	{
		// Give extra penalty if a non-oper queries the /RULES of a remote server
		LocalUser* localuser = IS_LOCAL(user);
		if ((localuser) && (!IS_OPER(user)))
			localuser->CommandFloodPenalty += 2000;
		return CMD_SUCCESS;
	}

	ConfigTag* tag = ServerInstance->Config->EmptyTag;
	if (IS_LOCAL(user))
		tag = user->GetClass()->config;
	std::string rules_name = tag->getString("rules", "rules");
	ConfigFileCache::iterator rules = ServerInstance->Config->Files.find(rules_name);
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
