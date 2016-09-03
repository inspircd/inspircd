/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2007-2008, 2012 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

#include "main.h"
#include "utils.h"
#include "commands.h"
#include "treeserver.h"

CmdResult CommandNick::HandleRemote(::RemoteUser* user, std::vector<std::string>& params)
{
	if ((isdigit(params[0][0])) && (params[0] != user->uuid))
		throw ProtocolException("Attempted to change nick to an invalid or non-matching UUID");

	// Timestamp of the new nick
	time_t newts = ServerCommand::ExtractTS(params[1]);

	/*
	 * On nick messages, check that the nick doesn't already exist here.
	 * If it does, perform collision logic.
	 */
	User* x = ServerInstance->FindNickOnly(params[0]);
	if ((x) && (x != user) && (x->registered == REG_ALL))
	{
		// 'x' is the already existing user using the same nick as params[0]
		// 'user' is the user trying to change nick to the in use nick
		bool they_change = Utils->DoCollision(x, TreeServer::Get(user), newts, user->ident, user->GetIPString(), user->uuid, "NICK");
		if (they_change)
		{
			// Remote client lost, or both lost, rewrite this nick change as a change to uuid before
			// calling ChangeNick() and forwarding the message
			params[0] = user->uuid;
			params[1] = ConvToStr(CommandSave::SavedTimestamp);
			newts = CommandSave::SavedTimestamp;
		}
	}

	user->ChangeNick(params[0], newts);

	return CMD_SUCCESS;
}
