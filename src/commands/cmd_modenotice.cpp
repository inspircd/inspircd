/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

class CommandModeNotice : public Command
{
 public:
	CommandModeNotice(Module* parent) : Command(parent,"MODENOTICE",2,2)
	{
		syntax = "<modes> <message>";
		flags_needed = 'o';
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *src)
	{
		int mlen = parameters[0].length();
		for (LocalUserList::const_iterator i = ServerInstance->Users->local_users.begin(); i != ServerInstance->Users->local_users.end(); i++)
		{
			User* user = *i;
			for (int n = 0; n < mlen; n++)
			{
				if (!user->IsModeSet(parameters[0][n]))
					goto next_user;
			}
			user->Write(":%s NOTICE %s :*** From %s: %s", ServerInstance->Config->ServerName.c_str(),
				user->nick.c_str(), src->nick.c_str(), parameters[1].c_str());
next_user:	;
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

COMMAND_INIT(CommandModeNotice)
