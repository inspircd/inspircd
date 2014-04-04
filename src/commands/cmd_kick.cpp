/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

/** Handle /KICK. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandKick : public Command
{
 public:
	/** Constructor for kick.
	 */
	CommandKick ( Module* parent) : Command(parent,"KICK",2,3) { syntax = "<channel> <nick>{,<nick>} [<reason>]"; }
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /KICK
 */
CmdResult CommandKick::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string reason;
	Channel* c = ServerInstance->FindChan(parameters[0]);
	User* u;

	if (ServerInstance->Parser->LoopCall(user, this, parameters, 1))
		return CMD_SUCCESS;

	if (IS_LOCAL(user))
		u = ServerInstance->FindNickOnly(parameters[1]);
	else
		u = ServerInstance->FindNick(parameters[1]);

	if ((!u) || (!c) || (u->registered != REG_ALL))
	{
		user->WriteServ( "401 %s %s :No such nick/channel", user->nick.c_str(), c ? parameters[1].c_str() : parameters[0].c_str());
		return CMD_FAILURE;
	}

	if ((IS_LOCAL(user)) && (!c->HasUser(user)) && (!ServerInstance->ULine(user->server)))
	{
		user->WriteServ( "442 %s %s :You're not on that channel!", user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	if (parameters.size() > 2)
	{
		reason.assign(parameters[2], 0, ServerInstance->Config->Limits.MaxKick);
	}
	else
	{
		reason.assign(user->nick, 0, ServerInstance->Config->Limits.MaxKick);
	}

	c->KickUser(user, u, reason.c_str());

	return CMD_SUCCESS;
}

COMMAND_INIT(CommandKick)
