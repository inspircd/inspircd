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

/** Handle /ISON. These command handlers can be reloaded by the core,
 * and handle basic RFC1459 commands. Commands within modules work
 * the same way, however, they can be fully unloaded, where these
 * may not.
 */
class CommandIson : public Command
{
 public:
	/** Constructor for ison.
	 */
	CommandIson ( Module* parent) : Command(parent,"ISON", 1) {
		syntax = "<nick> {nick}";
	}
	/** Handle command.
	 * @param parameters The parameters to the comamnd
	 * @param pcnt The number of parameters passed to teh command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

/** Handle /ISON
 */
CmdResult CommandIson::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::map<User*,User*> ison_already;
	User *u;
	std::string reply = "303 " + user->nick + " :";

	for (unsigned int i = 0; i < parameters.size(); i++)
	{
		u = ServerInstance->FindNickOnly(parameters[i]);
		if (ison_already.find(u) != ison_already.end())
			continue;

		if ((u) && (u->registered == REG_ALL))
		{
			reply.append(u->nick).append(" ");
			if (reply.length() > 450)
			{
				user->WriteServ(reply);
				reply = "303 " + user->nick + " :";
			}
			ison_already[u] = u;
		}
		else
		{
			if ((i == parameters.size() - 1) && (parameters[i].find(' ') != std::string::npos))
			{
				/* Its a space seperated list of nicks (RFC1459 says to support this)
				 */
				irc::spacesepstream list(parameters[i]);
				std::string item;

				while (list.GetToken(item))
				{
					u = ServerInstance->FindNickOnly(item);
					if (ison_already.find(u) != ison_already.end())
						continue;

					if ((u) && (u->registered == REG_ALL))
					{
						reply.append(u->nick).append(" ");
						if (reply.length() > 450)
						{
							user->WriteServ(reply);
							reply = "303 " + user->nick + " :";
						}
						ison_already[u] = u;
					}
				}
			}
		}
	}

	if (!reply.empty())
		user->WriteServ(reply);

	return CMD_SUCCESS;
}


COMMAND_INIT(CommandIson)
