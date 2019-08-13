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
	, operquit(parent, "operquit", ExtensionItem::EXT_USER, true)
{
	works_before_reg = true;
	syntax = "[:<message>]";
}

CmdResult CommandQuit::Handle(User* user, const Params& parameters)
{
	std::string quitmsg;
	if (parameters.empty())
		quitmsg = "Client exited";
	else if (IS_LOCAL(user))
		msgwrap.Wrap(parameters[0], quitmsg);
	else
		quitmsg = parameters[0];

	std::string* operquitmsg = operquit.get(user);
	ServerInstance->Users.QuitUser(user, quitmsg, operquitmsg);

	return CMD_SUCCESS;
}

RouteDescriptor CommandQuit::GetRouting(User* user, const Params& parameters)
{
	return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
