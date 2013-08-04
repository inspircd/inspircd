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
CmdResult CommandFJoin::Handle(const std::vector<std::string>& params, User *srcuser)
{
	SpanningTreeUtilities* Utils = ((ModuleSpanningTree*)(Module*)creator)->Utils;
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
	 * :<sid> FJOIN <chan> <TS> <modes> :[[modes,]<uuid> [[modes,]<uuid> ... ]]
	 * The last parameter is a list consisting of zero or more (modelist, uuid)
	 * pairs (permanent channels may have zero users). The mode list for each
	 * user is a concatenation of the mode letters the user has on the channel
	 * (e.g.: "ov" if the user is opped and voiced). The order of the mode letters
	 * are not important but if a server ecounters an unknown mode letter, it will
	 * drop the link to avoid desync.
	 *
	 * InspIRCd 2.0 and older required a comma before the uuid even if the user
	 * had no prefix modes on the channel, InspIRCd 2.2 and later does not require
	 * a comma in this case anymore.
	 *
	 */

	time_t TS = ConvToInt(params[1]);
	if (!TS)
	{
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "*** BUG? *** TS of 0 sent to FJOIN. Are some services authors smoking craq, or is it 1970 again?. Dropped.");
		ServerInstance->SNO->WriteToSnoMask('d', "WARNING: The server %s is sending FJOIN with a TS of zero. Total craq. Command was dropped.", srcuser->server.c_str());
		return CMD_INVALID;
	}

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
				/* Our TS greater than theirs, clear all our modes from the channel, accept theirs. */
				if (Utils->AnnounceTSChange)
					chan->WriteChannelWithServ(ServerInstance->Config->ServerName, "NOTICE %s :TS for %s changed from %lu to %lu", chan->name.c_str(), channel.c_str(), (unsigned long) ourTS, (unsigned long) TS);

				// while the name is equal in case-insensitive compare, it might differ in case; use the remote version
				chan->name = channel;
				chan->age = TS;
				chan->ClearInvites();

				CommandFJoin::RemoveStatus(chan);

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
	if (apply_other_sides_modes)
	{
		std::vector<std::string> modelist;
		modelist.push_back(channel);

		/* Remember, params[params.size() - 1] is userlist, and we don't want to apply *that* */
		modelist.insert(modelist.end(), params.begin()+2, params.end()-1);
		ServerInstance->Modes->Process(modelist, srcuser, ModeParser::MODE_LOCALONLY | ModeParser::MODE_MERGE);
	}

	irc::modestacker modestack(true);
	TreeSocket* src_socket = Utils->FindServer(srcuser->server)->GetRoute()->GetSocket();

	/* Now, process every 'modes,uuid' pair */
	irc::tokenstream users(*params.rbegin());
	std::string item;
	irc::modestacker* modestackptr = (apply_other_sides_modes ? &modestack : NULL);
	while (users.GetToken(item))
	{
		if (!ProcessModeUUIDPair(item, src_socket, chan, modestackptr))
			return CMD_INVALID;
	}

	/* Flush mode stacker if we lost the FJOIN or had equal TS */
	if (apply_other_sides_modes)
		CommandFJoin::ApplyModeStack(srcuser, chan, modestack);

	return CMD_SUCCESS;
}

bool CommandFJoin::ProcessModeUUIDPair(const std::string& item, TreeSocket* src_socket, Channel* chan, irc::modestacker* modestack)
{
	std::string::size_type comma = item.find(',');

	// Comma not required anymore if the user has no modes
	std::string uuid = ((comma == std::string::npos) ? item : item.substr(comma+1));
	User* who = ServerInstance->FindUUID(uuid);
	if (!who)
	{
		// Probably KILLed, ignore
		return true;
	}

	/* Check that the user's 'direction' is correct */
	SpanningTreeUtilities* Utils = ((ModuleSpanningTree*)(Module*)creator)->Utils;
	TreeServer* route_back_again = Utils->BestRouteTo(who->server);
	if ((!route_back_again) || (route_back_again->GetSocket() != src_socket))
	{
		return true;
	}

	/* Check if the user received at least one mode */
	if ((modestack) && (comma > 0) && (comma != std::string::npos))
	{
		/* Iterate through the modes and see if they are valid here, if so, apply */
		std::string::const_iterator commait = item.begin()+comma;
		for (std::string::const_iterator i = item.begin(); i != commait; ++i)
		{
			if (!ServerInstance->Modes->FindMode(*i, MODETYPE_CHANNEL))
			{
				ServerInstance->SNO->WriteToSnoMask('d', "Unrecognised mode '%c' for a user in FJOIN, dropping link", *i);
				return false;
			}

			/* Add any modes this user had to the mode stack */
			modestack->Push(*i, who->nick);
		}
	}

	chan->ForceJoin(who, NULL, route_back_again->bursting);
	return true;
}

void CommandFJoin::RemoveStatus(Channel* c)
{
	irc::modestacker stack(false);

	for (char modeletter = 'A'; modeletter <= 'z'; ++modeletter)
	{
		ModeHandler* mh = ServerInstance->Modes->FindMode(modeletter, MODETYPE_CHANNEL);

		/* Passing a pointer to a modestacker here causes the mode to be put onto the mode stack,
		 * rather than applied immediately. Module unloads require this to be done immediately,
		 * for this function we require tidyness instead. Fixes bug #493
		 */
		if (mh)
			mh->RemoveMode(c, stack);
	}

	ApplyModeStack(ServerInstance->FakeClient, c, stack);
}

void CommandFJoin::ApplyModeStack(User* srcuser, Channel* c, irc::modestacker& stack)
{
	parameterlist stackresult;
	stackresult.push_back(c->name);

	while (stack.GetStackedLine(stackresult))
	{
		ServerInstance->Modes->Process(stackresult, srcuser, ModeParser::MODE_LOCALONLY);
		stackresult.erase(stackresult.begin() + 1, stackresult.end());
	}
}
