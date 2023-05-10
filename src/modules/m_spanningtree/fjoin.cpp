/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015, 2018-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008, 2012 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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
#include "timeutils.h"

#include "commands.h"
#include "treeserver.h"
#include "treesocket.h"

/** FJOIN builder for rebuilding incoming FJOINs and splitting them up into multiple messages if necessary
 */
class FwdFJoinBuilder final
	: public CommandFJoin::Builder
{
public:
	FwdFJoinBuilder(Channel* chan, TreeServer* server)
		: CommandFJoin::Builder(chan, server)
	{
	}

	void add(Membership* memb, std::string::const_iterator mbegin, std::string::const_iterator mend);
};

/** FJOIN, almost identical to TS6 SJOIN, except for nicklist handling. */
CmdResult CommandFJoin::Handle(User* srcuser, Params& params)
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
	 * If the timestamps are exactly equal, both sides merge their privileges
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
	 * Outside of netbursts, the winning side also resyncs the losing side if it
	 * detects that the other side recreated the channel.
	 *
	 * Syntax:
	 * :<sid> FJOIN <chan> <TS> <modes> :[<member> [<member> ...]]
	 * The last parameter is a list consisting of zero or more channel members
	 * (permanent channels may have zero users). Each entry on the list is in the
	 * following format:
	 * [[<modes>,]<uuid>[:<membid>]
	 * <modes> is a concatenation of the mode letters the user has on the channel
	 * (e.g.: "ov" if the user is opped and voiced). The order of the mode letters
	 * are not important but if a server encounters an unknown mode letter, it will
	 * drop the link to avoid desync.
	 *
	 * InspIRCd 2.0 and older required a comma before the uuid even if the user
	 * had no prefix modes on the channel, InspIRCd 3.0 and later does not require
	 * a comma in this case anymore.
	 *
	 * <membid> is a positive integer representing the id of the membership.
	 * If not present (in FJOINs coming from pre-1205 servers), 0 is assumed.
	 *
	 * Forwarding:
	 * FJOIN messages are forwarded with the new TS and modes. Prefix modes of
	 * members on the losing side are not forwarded.
	 * This is required to only have one server on each side of the network who
	 * decides the fate of a channel during a network merge. Otherwise, if the
	 * clock of a server is slightly off it may make a different decision than
	 * the rest of the network and desync.
	 * The prefix modes are always forwarded as-is, or not at all.
	 *
	 * Forwarding examples:
	 * Existing channel #chan with TS 1000, modes +n.
	 * Incoming:  :220 FJOIN #chan 1000 +t :o,220AAAAAB:0
	 * Forwarded: :220 FJOIN #chan 1000 +nt :o,220AAAAAB:0
	 * Merge modes and forward the result. Forward their prefix modes as well.
	 *
	 * Existing channel #chan with TS 1000, modes +nt.
	 * Incoming:  :220 FJOIN #CHAN 2000 +i :ov,220AAAAAB:0 o,220AAAAAC:20
	 * Forwarded: :220 FJOIN #chan 1000 +nt :,220AAAAAB:0 ,220AAAAAC:20
	 * Drop their modes, forward our modes and TS, use our channel name
	 * capitalization. Don't forward prefix modes.
	 *
	 */

	time_t TS = ServerCommand::ExtractTS(params[1]);

	const std::string& channel = params[0];
	auto* chan = ServerInstance->Channels.Find(channel);
	bool apply_other_sides_modes = true;
	TreeServer* const sourceserver = TreeServer::Get(srcuser);

	if (!chan)
	{
		chan = new Channel(channel, TS);
	}
	else
	{
		time_t ourTS = chan->age;
		if (TS != ourTS)
		{
			ServerInstance->Logs.Debug(MODNAME, "Merge FJOIN received for {}, ourTS: {}, TS: {}, difference: {}",
				chan->name, ourTS, TS, ourTS - TS);
			/* If our TS is less than theirs, we dont accept their modes */
			if (ourTS < TS)
			{
				// If the source server isn't bursting then this FJOIN is the result of them recreating the channel with a higher TS.
				// This happens if the last user on the channel hops and before the PART propagates a user on another server joins. Fix it by doing a resync.
				// Servers behind us won't react this way because the forwarded FJOIN will have the correct TS.
				if (!sourceserver->IsBursting())
				{
					ServerInstance->Logs.Debug(MODNAME, "Server {} recreated channel {} with higher TS, resyncing", sourceserver->GetName(), chan->name);
					sourceserver->GetSocket()->SyncChannel(chan);
				}
				apply_other_sides_modes = false;
			}
			else if (ourTS > TS)
			{
				// Our TS is greater than theirs, remove all modes, extensions, etc. from the channel
				LowerTS(chan, TS, channel);

				// XXX: If the channel does not exist in the chan hash at this point, create it so the remote modes can be applied on it.
				// This happens to 0-user permanent channels on the losing side, because those are removed (from the chan hash, then
				// deleted later) as soon as the permchan mode is removed from them.
				if (!ServerInstance->Channels.Find(channel))
				{
					chan = new Channel(channel, TS);
				}
			}
		}
	}

	// Apply their channel modes if we have to
	Modes::ChangeList modechangelist;
	if (apply_other_sides_modes)
	{
		ServerInstance->Modes.ModeParamsToChangeList(srcuser, MODETYPE_CHANNEL, params, modechangelist, 2, params.size() - 1);
		ServerInstance->Modes.Process(srcuser, chan, nullptr, modechangelist, ModeParser::MODE_LOCALONLY | ModeParser::MODE_MERGE);
		// Reuse for prefix modes
		modechangelist.clear();
	}

	// Build a new FJOIN for forwarding. Put the correct TS in it and the current modes of the channel
	// after applying theirs. If they lost, the prefix modes from their message are not forwarded.
	FwdFJoinBuilder fwdfjoin(chan, sourceserver);

	// Process every member in the message
	irc::spacesepstream users(params.back());
	std::string item;
	Modes::ChangeList* modechangelistptr = (apply_other_sides_modes ? &modechangelist : nullptr);
	while (users.GetToken(item))
	{
		ProcessModeUUIDPair(item, sourceserver, chan, modechangelistptr, fwdfjoin);
	}

	fwdfjoin.finalize();
	fwdfjoin.Forward(sourceserver->GetRoute());

	// Set prefix modes on their users if we lost the FJOIN or had equal TS
	if (apply_other_sides_modes)
		ServerInstance->Modes.Process(srcuser, chan, nullptr, modechangelist, ModeParser::MODE_LOCALONLY);

	return CmdResult::SUCCESS;
}

void CommandFJoin::ProcessModeUUIDPair(const std::string& item, TreeServer* sourceserver, Channel* chan, Modes::ChangeList* modechangelist, FwdFJoinBuilder& fwdfjoin)
{
	std::string::size_type comma = item.find(',');

	// Comma not required anymore if the user has no modes
	const std::string::size_type ubegin = (comma == std::string::npos ? 0 : comma+1);
	std::string uuid(item, ubegin, UIDGenerator::UUID_LENGTH);
	auto* who = ServerInstance->Users.FindUUID(uuid);
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
			ModeHandler* mh = ServerInstance->Modes.FindMode(*i, MODETYPE_CHANNEL);
			if (!mh)
				throw ProtocolException("Unrecognised mode '" + std::string(1, *i) + "'");

			/* Add any modes this user had to the mode stack */
			modechangelist->push_add(mh, who->nick);
		}
	}

	Membership* memb = chan->ForceJoin(who, nullptr, sourceserver->IsBursting());
	if (!memb)
	{
		// User was already on the channel, forward because of the modes they potentially got
		memb = chan->GetUser(who);
		if (memb)
			fwdfjoin.add(memb, item.begin(), modeendit);
		return;
	}

	// Assign the id to the new Membership
	Membership::Id membid = 0;
	const std::string::size_type colon = item.rfind(':');
	if (colon != std::string::npos)
		membid = Membership::IdFromString(item.substr(colon+1));
	memb->id = membid;

	// Add member to fwdfjoin with prefix modes
	fwdfjoin.add(memb, item.begin(), modeendit);
}

void CommandFJoin::RemoveStatus(Channel* c)
{
	Modes::ChangeList changelist;

	for (const auto& [_, mh] : ServerInstance->Modes.GetModes(MODETYPE_CHANNEL))
	{
		// Add the removal of this mode to the changelist. This handles all kinds of modes, including prefix modes.
		mh->RemoveMode(c, changelist);
	}

	ServerInstance->Modes.Process(ServerInstance->FakeClient, c, nullptr, changelist, ModeParser::MODE_LOCALONLY);
}

void CommandFJoin::LowerTS(Channel* chan, time_t TS, const std::string& newname)
{
	if (Utils->AnnounceTSChange)
	{
		// WriteRemoteNotice is not used here because the message only needs to go to the local server.
		chan->WriteNotice(INSP_FORMAT("Creation time of {} changed from {} to {}", newname,
			Time::ToString(chan->age), Time::ToString(TS)));
	}

	// While the name is equal in case-insensitive compare, it might differ in case; use the remote version
	chan->name = newname;
	chan->age = TS;

	// Clear all modes
	CommandFJoin::RemoveStatus(chan);

	// Unset all extensions
	chan->FreeAllExtItems();

	// Clear the topic
	chan->SetTopic(ServerInstance->FakeClient, std::string(), 0);
	chan->setby.clear();
}

CommandFJoin::Builder::Builder(Channel* chan, TreeServer* source)
	: CmdBuilder(source, "FJOIN")
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

void CommandFJoin::Builder::clear()
{
	content.erase(pos);
	push_raw(" :");
}

const std::string& CommandFJoin::Builder::finalize()
{
	if (content.back() == ' ')
		content.pop_back();
	return str();
}

void FwdFJoinBuilder::add(Membership* memb, std::string::const_iterator mbegin, std::string::const_iterator mend)
{
	// Add the member and their modes exactly as they sent them
	CommandFJoin::Builder::add(memb, mbegin, mend);
}
