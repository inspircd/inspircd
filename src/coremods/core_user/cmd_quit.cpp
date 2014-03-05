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
#include "core_user.h"

CommandQuit::CommandQuit(Module* parent)
	: Command(parent, "QUIT", 0, 1)
{
	works_before_reg = true;
	syntax = "[<message>]";
}

CmdResult CommandQuit::Handle (const std::vector<std::string>& parameters, User *user)
{

	std::string quitmsg;

	if (IS_LOCAL(user))
	{
		if (!ServerInstance->Config->FixedQuit.empty())
			quitmsg = ServerInstance->Config->FixedQuit;
		else
			quitmsg = parameters.size() ?
				ServerInstance->Config->PrefixQuit + parameters[0] + ServerInstance->Config->SuffixQuit
				: "Client exited";
	}
	else
		quitmsg = parameters.size() ? parameters[0] : "Client exited";

	std::string* operquit = ServerInstance->OperQuit.get(user);
	ServerInstance->Users->QuitUser(user, quitmsg, operquit);

	return CMD_SUCCESS;
}

RouteDescriptor CommandQuit::GetRouting(User* user, const std::vector<std::string>& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
