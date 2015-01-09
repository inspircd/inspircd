/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
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
#include "commands.h"
#include "treeserver.h"
#include "treesocket.h"

/** FJOIN, almost identical to TS6 SJOIN, except for nicklist handling. */
CmdResult CommandFJoin::Handle(User* srcuser, std::vector<std::string>& params)
{
	/* 1.1+ FJOIN works as follows:
	 *
	 * Each FJOIN is sent along with a timestamp, and the side with the lowest
	 * timestamp 'wins'. From this point on we will refer to this side as the
	 * winner. The side with the higher timestamp loses, from this point on we
	 * will call this side the loser or losing side. This should be familiar to
	 * anyone who's dealt with dreamforge or TS6 before.
	 *
	 * When two sides of a split heal and this occurs, the following things
	 * will happen:
	 *
	 * If the timestamps are exactly equal, both sides merge their privilages
	 * and users, as in InspIRCd 1.0 and ircd2.8. The channels have not been
	 * re-created during a split, this is safe to do.
	 *
	 * If the timestamps are NOT equal, the losing side removes all of its
	 * modes from the channel, before introducing new users into the channel
	 * which are listed in the FJOIN command's parameters. The losing side then
	 * LOWERS its timestamp value of the channel to match that of the winning
	 * side, and the modes of the users of the winning side are merged in with
	 * the losing side.
	 *
	 * The winning side on the other hand will ignore all user modes from the
	 * losing side, so only its own modes get applied. Life is simple for those
	 * who succeed at internets. :-)
	 *
	 * Syntax:
	 * :<sid> FJOIN <chan> <TS> <modes> :[<member> [<member> ...]]
	 * The last parameter is a list consisting of zero or more channel members
	 * (permanent channels may have zero users). Each entry on the list is in the
	 * following format:
	 * [[<modes>,]<uuid>[:<membid>]
	 * <modes> is a concatenation of the mode letters the user has on the channel
	 * (e.g.: "ov" if the user is opped and voiced). The order of the mode letters
	 * are not important but if a server ecounters an unknown mode letter, it will
	 * drop the link to avoid desync.
	 *
	 * InspIRCd 2.0 and older required a comma before the uuid even if the user
	 * had no prefix modes on the channel, InspIRCd 2.2 and later does not require
	 * a comma in this case anymore.
	 *
	 * <membid> is a positive integer representing the id of the membership.
	 * If not present (in FJOINs coming from pre-1205 servers), 0 is assumed.
	 *
	 */

	time_t TS = ServerCommand::ExtractTS(params[1]);

	const std::string& channel = params[0];
	Channel* chan = ServerInstance->FindChan(channel);
	bool apply_other_sides_modes = true;

	if (!chan)
	{
		chan = new Channel(channel, TS);
	}
	else
	{
		time_t ourTS = chan->age;
		if (TS != ourTS)
		{
			ServerInstance->SNO->WriteToSnoMask('d', "Merge FJOIN received for %s, ourTS: %lu, TS: %lu, difference: %lu",
				chan->name.c_str(), (unsigned long)ourTS, (unsigned long)TS, (unsigned long)(ourTS - TS));
			/* If our TS is less than theirs, we dont accept their modes */
			if (ourTS < TS)
			{
				apply_other_sides_modes = false;
			}
			else if (ourTS > TS)
			{
				// Our TS is greater than theirs, remove all modes, extensions, etc. from the channel
				LowerTS(chan, TS, channel);

				// XXX: If the channel does not exist in the chan hash at this point, create it so the remote modes can be applied on it.
				// This happens to 0-user permanent channels on the losing side, because those are removed (from the chan hash, then
				// deleted later) as soon as the permchan mode is removed from them.
				if (ServerInstance->FindChan(channel) == NULL)
				{
					chan = new Channel(channel, TS);
				}
			}
		}
	}

	/* First up, apply their channel modes if they won the TS war */
	Modes::ChangeList modechangelist;
	if (apply_other_sides_modes)
	{
		ServerInstance->Modes.ModeParamsToChangeList(srcuser, MODETYPE_CHANNEL, params, modechangelist, 2, params.size() - 1);
		ServerInstance->Modes->Process(srcuser, chan, NULL, modechangelist, ModeParser::MODE_LOCALONLY | ModeParser::MODE_MERGE);
		// Reuse for prefix modes
		modechangelist.clear();
	}

	TreeServer* const sourceserver = TreeServer::Get(srcuser);

	/* Now, process every 'modes,uuid' pair */
	irc::tokenstream users(params.back());
	std::string item;
	Modes::ChangeList* modechangelistptr = (apply_other_sides_modes ? &modechangelist : NULL);
	while (users.GetToken(item))
	{
		ProcessModeUUIDPair(item, sourceserver, chan, modechangelistptr);
	}

	// Set prefix modes on their users if we lost the FJOIN or had equal TS
	if (apply_other_sides_modes)
		ServerInstance->Modes->Process(srcuser, chan, NULL, modechangelist, ModeParser::MODE_LOCALONLY);

	return CMD_SUCCESS;
}

void CommandFJoin::ProcessModeUUIDPair(const std::string& item, TreeServer* sourceserver, Channel* chan, Modes::ChangeList* modechangelist)
{
	std::string::size_type comma = item.find(',');

	// Comma not required anymore if the user has no modes
	const std::string::size_type ubegin = (comma == std::string::npos ? 0 : comma+1);
	std::string uuid(item, ubegin, UIDGenerator::UUID_LENGTH);
	User* who = ServerInstance->FindUUID(uuid);
	if (!who)
	{
		// Probably KILLed, ignore
		return;
	}

	TreeSocket* src_socket = sourceserver->GetSocket();
	/* Check that the user's 'direction' is correct */
	TreeServer* route_back_again = TreeServer::Get(who);
	if (route_back_again->GetSocket() != src_socket)
	{
		return;
	}

	std::string::const_iterator modeendit = item.begin(); // End of the "ov" mode string
	/* Check if the user received at least one mode */
	if ((modechangelist) && (comma != std::string::npos))
	{
		modeendit += comma;
		/* Iterate through the modes and see if they are valid here, if so, apply */
		for (std::string::const_iterator i = item.begin(); i != modeendit; ++i)
		{
			ModeHandler* mh = ServerInstance->Modes->FindMode(*i, MODETYPE_CHANNEL);
			if (!mh)
				throw ProtocolException("Unrecognised mode '" + std::string(1, *i) + "'");

			/* Add any modes this user had to the mode stack */
			modechangelist->push_add(mh, who->nick);
		}
	}

	Membership* memb = chan->ForceJoin(who, NULL, sourceserver->IsBursting());
	if (!memb)
		return;

	// Assign the id to the new Membership
	Membership::Id membid = 0;
	const std::string::size_type colon = item.rfind(':');
	if (colon != std::string::npos)
		membid = Membership::IdFromString(item.substr(colon+1));
	memb->id = membid;
}

void CommandFJoin::RemoveStatus(Channel* c)
{
	Modes::ChangeList changelist;

	const ModeParser::ModeHandlerMap& mhs = ServerInstance->Modes->GetModes(MODETYPE_CHANNEL);
	for (ModeParser::ModeHandlerMap::const_iterator i = mhs.begin(); i != mhs.end(); ++i)
	{
		ModeHandler* mh = i->second;

		/* Passing a pointer to a modestacker here causes the mode to be put onto the mode stack,
		 * rather than applied immediately. Module unloads require this to be done immediately,
		 * for this function we require tidyness instead. Fixes bug #493
		 */
		mh->RemoveMode(c, changelist);
	}

	ServerInstance->Modes->Process(ServerInstance->FakeClient, c, NULL, changelist, ModeParser::MODE_LOCALONLY);
}

void CommandFJoin::LowerTS(Channel* chan, time_t TS, const std::string& newname)
{
	if (Utils->AnnounceTSChange)
		chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :TS for %s changed from %lu to %lu", chan->name.c_str(), newname.c_str(), (unsigned long) chan->age, (unsigned long) TS);

	// While the name is equal in case-insensitive compare, it might differ in case; use the remote version
	chan->name = newname;
	chan->age = TS;

	// Remove all pending invites
	chan->ClearInvites();

	// Clear all modes
	CommandFJoin::RemoveStatus(chan);

	// Unset all extensions
	chan->FreeAllExtItems();

	// Clear the topic, if it isn't empty then send a topic change message to local users
	if (!chan->topic.empty())
	{
		chan->topic.clear();
		chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "TOPIC %s :", chan->name.c_str());
	}
	chan->setby.clear();
	chan->topicset = 0;
}

CommandFJoin::Builder::Builder(Channel* chan, TreeServer* source)
	: CmdBuilder(source->GetID(), "FJOIN")
{
	push(chan->name).push_int(chan->age).push_raw(" +");
	pos = str().size();
	push_raw(chan->ChanModes(true)).push_raw(" :");
}

void CommandFJoin::Builder::add(Membership* memb, std::string::const_iterator mbegin, std::string::const_iterator mend)
{
	push_raw(mbegin, mend).push_raw(',').push_raw(memb->user->uuid);
	push_raw(':').push_raw_int(memb->id);
	push_raw(' ');
}

bool CommandFJoin::Builder::has_room(std::string::size_type nummodes) const
{
	return ((str().size() + nummodes + UIDGenerator::UUID_LENGTH + 2 + membid_max_digits + 1) <= maxline);
}

void CommandFJoin::Builder::clear()
{
	content.erase(pos);
	push_raw(" :");
}

const std::string& CommandFJoin::Builder::finalize()
{
	if (*content.rbegin() == ' ')
		content.erase(content.size()-1);
	return str();
}
