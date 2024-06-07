/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008, 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"
#include "commandbuilder.h"
#include "commands.h"

/*
 * Yes, this function looks a little ugly.
 * However, in some circumstances we may not have a User, so we need to do things this way.
 * Returns true if remote or both lost, false otherwise.
 * Sends SAVEs as appropriate and forces nick change of the user 'u' if our side loses or if both lose.
 * Does not change the nick of the user that is trying to claim the nick of 'u', i.e. the "remote" user.
 */
bool SpanningTreeUtilities::DoCollision(User* u, TreeServer* server, time_t remotets, const std::string& remoteuser, const std::string& remoteip, const std::string& remoteuid, const char* collidecmd)
{
	// At this point we're sure that a collision happened, increment the counter regardless of who wins
	ServerInstance->Stats.Collisions++;

	/*
	 * Under old protocol rules, we would have had to kill both clients.
	 * Really, this sucks.
	 * These days, we have UID. And, so what we do is, force nick change client(s)
	 * involved according to timestamp rules.
	 *
	 * RULES:
	 *  user@ip equal:
	 *   Force nick change on OLDER timestamped client
	 *  user@ip differ:
	 *   Force nick change on NEWER timestamped client
	 *  TS EQUAL:
	 *   FNC both.
	 *
	 * This stops abusive use of collisions, simplifies problems with loops, and so on.
	 *   -- w00t
	 */
	bool bChangeLocal = true;
	bool bChangeRemote = true;

	// If the timestamps are not equal only one of the users has to change nick,
	// otherwise both have to change
	const time_t localts = u->nickchanged;
	if (remotets != localts)
	{
		/* first, let's see if user@host matches. */
		const std::string& localuser = u->GetRealUser();
		const std::string& localip = u->GetAddress();
		bool SamePerson = (localuser == remoteuser)
				&& (localip == remoteip);

		/*
		 * if user@ip is equal, and theirs is newer, or
		 * user@ip differ, and ours is newer
		 */
		if ((SamePerson && remotets < localts) || (!SamePerson && remotets > localts))
		{
			// Only remote needs to change
			bChangeLocal = false;
		}
		else
		{
			// Only ours needs to change
			bChangeRemote = false;
		}
	}

	ServerInstance->Logs.Debug(MODNAME, "Nick collision on \"{}\" caused by {}: {}/{}/{}@{} {} <-> {}/{}/{}@{} {}", u->nick, collidecmd,
		u->uuid, localts, u->GetRealUser(), u->GetAddress(), bChangeLocal,
		remoteuid, remotets, remoteuser, remoteip, bChangeRemote);

	/*
	 * Send SAVE and accept the losing client with its UID (as we know the SAVE will
	 * not fail under any circumstances -- UIDs are netwide exclusive).
	 *
	 * This means that each side of a collide will generate one extra NICK back to where
	 * they have just linked (and where it got the SAVE from), however, it will
	 * be dropped harmlessly as it will come in as :928AAAB NICK 928AAAB, and we already
	 * have 928AAAB's nick set to that.
	 *   -- w00t
	 */

	if (bChangeLocal)
	{
		/*
		 * Local-side nick needs to change. Just in case we are hub, and
		 * this "local" nick is actually behind us, send a SAVE out.
		 */
		CmdBuilder params("SAVE");
		params.push(u->uuid);
		params.push(ConvToStr(u->nickchanged));
		params.Broadcast();

		u->ChangeNick(u->uuid, CommandSave::SavedTimestamp);
	}
	if (bChangeRemote)
	{
		/*
		 * Remote side needs to change. If this happens, we modify the UID or NICK and
		 * send back a SAVE to the source.
		 */
		CmdBuilder("SAVE").push(remoteuid).push_int(remotets).Unicast(server->ServerUser);
	}

	return bChangeRemote;
}
