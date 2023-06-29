/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
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

CmdResult CommandNick::HandleRemote(::RemoteUser* user, Params& params)
{
	if ((isdigit(params[0][0])) && (params[0] != user->uuid))
		throw ProtocolException("Attempted to change nick to an invalid or non-matching UUID");

	// Timestamp of the new nick
	time_t newts = ServerCommand::ExtractTS(params[1]);

	/*
	 * On nick messages, check that the nick doesn't already exist here.
	 * If it does, perform collision logic.
	 */
	auto* x = ServerInstance->Users.FindNick(params[0], true);
	if (x && x != user)
	{
		// 'x' is the already existing user using the same nick as params[0]
		// 'user' is the user trying to change nick to the in use nick
		bool they_change = SpanningTreeUtilities::DoCollision(x, TreeServer::Get(user), newts, user->GetRealUser(), user->GetAddress(), user->uuid, "NICK");
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

	return CmdResult::SUCCESS;
}
