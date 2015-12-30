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

/** Handle /ISON.
 */
class CommandIson : public Command
{
	/** Helper function to append a nick to an ISON reply
	 * @param user User doing the /ISON
	 * @param toadd User to append to the ISON reply
	 * @param reply Reply string to append the nick to
	 * @param pos If the reply gets too long it is sent to the user and truncated from this position
	 */
	static bool AddNick(User* user, User* toadd, std::string& reply, const std::string::size_type pos);

 public:
	/** Constructor for ison.
	 */
	CommandIson ( Module* parent) : Command(parent,"ISON", 1) {
		syntax = "<nick> {nick}";
	}
	/** Handle command.
	 * @param parameters The parameters to the command
	 * @param user The user issuing the command
	 * @return A value from CmdResult to indicate command success or failure.
	 */
	CmdResult Handle(const std::vector<std::string>& parameters, User *user);
};

bool CommandIson::AddNick(User* user, User* toadd, std::string& reply, const std::string::size_type pos)
{
	if ((toadd) && (toadd->registered == REG_ALL))
	{
		reply.append(toadd->nick).push_back(' ');
		if (reply.length() > 450)
		{
			user->WriteServ(reply);
			reply.erase(pos);
		}
		return true;
	}
	return false;
}

/** Handle /ISON
 */
CmdResult CommandIson::Handle (const std::vector<std::string>& parameters, User *user)
{
	std::string reply = "303 " + user->nick + " :";
	const std::string::size_type pos = reply.size();

	for (std::vector<std::string>::const_iterator i = parameters.begin(); i != parameters.end()-1; ++i)
	{
		const std::string& targetstr = *i;

		User* const u = ServerInstance->FindNickOnly(targetstr);
		AddNick(user, u, reply, pos);
	}

	// Last parameter can be a space separated list
	irc::spacesepstream ss(parameters.back());
	for (std::string token; ss.GetToken(token); )
		AddNick(user, ServerInstance->FindNickOnly(token), reply, pos);

	user->WriteServ(reply);
	return CMD_SUCCESS;
}


COMMAND_INIT(CommandIson)
