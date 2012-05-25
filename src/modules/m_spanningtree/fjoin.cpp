/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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
	/* 1.1 FJOIN works as follows:
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
	 */
	if (params.size() < 3)
		return CMD_INVALID;

	irc::modestacker modestack;				/* Modes to apply from the users in the user list */
	User* who = NULL;		   				/* User we are currently checking */
	std::string channel = params[0];				/* Channel name, as a string */
	time_t TS = atoi(params[1].c_str());    			/* Timestamp given to us for remote side */
	irc::tokenstream users((params.size() > 3) ? params[params.size() - 1] : "");   /* users from the user list */
	bool apply_other_sides_modes = true;				/* True if we are accepting the other side's modes */
	Channel* chan = ServerInstance->FindChan(channel);		/* The channel we're sending joins to */
	bool incremental = (params[2] == "*");
	bool created = !chan;						/* True if the channel doesnt exist here yet */
	std::string item;						/* One item in the list of nicks */

	TreeSocket* src_socket = Utils->FindServer(srcuser->server)->GetSocket();

	if (!TS)
	{
		ServerInstance->SNO->WriteToSnoMask('d', "ERROR: The server %s sent an FJOIN with a TS of zero.", srcuser->server.c_str());
		return CMD_INVALID;
	}

	if (created)
	{
		chan = new Channel(channel, TS);
		if (incremental)
		{
			ServerInstance->SNO->WriteToSnoMask('d', "Incremental creation FJOIN received for %s, timestamp: %lu", chan->name.c_str(), (unsigned long)TS);
			parameterlist resync;
			resync.push_back(channel);
			Utils->DoOneToOne(ServerInstance->Config->GetSID().c_str(), "RESYNC", resync, srcuser->uuid);
		}
	}
	else
	{
		time_t ourTS = chan->age;

		if (TS != ourTS)
			ServerInstance->SNO->WriteToSnoMask('d', "Merge FJOIN received for %s, ourTS: %lu, TS: %lu, difference: %ld",
				chan->name.c_str(), (unsigned long)ourTS, (unsigned long)TS, (long)(ourTS - TS));
		/* If our TS is less than theirs, we dont accept their modes */
		if (ourTS < TS)
		{
			ServerInstance->SNO->WriteToSnoMask('d', "NOT Applying modes from other side");
			apply_other_sides_modes = false;
		}
		else if (ourTS > TS)
		{
			chan = Channel::Nuke(chan, channel, TS);
			if (incremental)
			{
				ServerInstance->SNO->WriteToSnoMask('d', "Incremental merge FJOIN received for %s", chan->name.c_str());
				parameterlist resync;
				resync.push_back(channel);
				Utils->DoOneToOne(ServerInstance->Config->GetSID().c_str(), "RESYNC", resync, srcuser->uuid);
			}
		}
		// The silent case here is ourTS == TS, we don't need to remove modes here, just to merge them later on.
	}

	/* First up, apply their modes if they won the TS war */
	if (apply_other_sides_modes && !incremental)
	{
		unsigned int idx = 2;
		std::vector<std::string> mode_list;

		// Mode parser needs to know what channel to act on.
		mode_list.push_back(params[0]);

		/* Remember, params[params.size() - 1] is nicklist, and we don't want to apply *that* */
		for (idx = 2; idx != (params.size() - 1); idx++)
		{
			mode_list.push_back(params[idx]);
		}

		ServerInstance->SendMode(mode_list, srcuser);
	}

	/* Now, process every 'modes,nick' pair */
	while (users.GetToken(item))
	{
		std::string::size_type comma = item.find(',');
		if (comma == std::string::npos)
			continue;

		std::string modes = item.substr(0, comma);

		/* Check the user actually exists */
		who = ServerInstance->FindUUID(item.substr(comma + 1));
		if (who)
		{
			/* Check that the user's 'direction' is correct */
			TreeServer* route_back_again = Utils->FindServer(who->server);
			if ((!route_back_again) || (route_back_again->GetSocket() != src_socket))
				continue;

			/* Add any modes this user had to the mode stack */
			for (std::string::iterator x = modes.begin(); x != modes.end(); ++x)
				modestack.push(irc::modechange(*x, MODETYPE_CHANNEL, who->uuid, true));

			Channel::JoinUser(who, channel.c_str(), true, "", route_back_again->bursting, TS);
		}
		else
		{
			ServerInstance->Logs->Log("m_spanningtree",SPARSE, "Ignored nonexistant user %s in fjoin to %s (probably quit?)", item.c_str(), channel.c_str());
			continue;
		}
	}

	/* Flush mode stacker if we lost the FJOIN or had equal TS */
	if (apply_other_sides_modes)
	{
		ServerInstance->SendMode(srcuser, chan, modestack, false);
	}
	return CMD_SUCCESS;
}
