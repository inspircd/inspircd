/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "commands.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/** FTOPIC command */
CmdResult CommandFTopic::Handle(const std::vector<std::string>& params, User *user)
{
	Channel* c = ServerInstance->FindChan(params[0]);
	if ((!c) || (!IS_SERVER(user)))
		return CMD_FAILURE;

	/**
	 * Here's how this works:
	 *
	 * In pre-1204 protocol version we had a syntax like
	 *
	 * FTOPIC #chan topicts setby :topic
	 *
	 * where topicts is the time when the topic was set, and setby is the n!u@h
	 * of the user who set it. If the topicts value was bigger (newer) than our topic
	 * TS (stored in Channel::topicset), we accepted the change, else we assumed
	 * we already had a more recent topic and did nothing.
	 *
	 * However this behavior meant that if the channel was recreated on a server
	 * during a netsplit, users there could set any topic and override ours with it
	 * at merge because their topic had a newer topic TS.
	 * Protocol version 1204 addresses this issue by adding the channel TS to FTOPIC:
	 *
	 * FTOPIC #chan chants topicts setby :topic
	 *
	 * so we notice when the channel has been recreated and discard the FTOPIC in that case.
	 * Apart from this the logic remains the same. For previous versions not supporting the
	 * new syntax we need to fall back to the old behavior.
	 *
	 */

	// Determine the protocol version of the sender
	SpanningTreeUtilities* Utils = ((ModuleSpanningTree*) (Module*) creator)->Utils;
	TreeServer* srcserver = Utils->FindServer(user->server);
	bool has_chants = (srcserver->Socket->proto_version >= 1204);

	// If we got a channel TS compare it with ours. If it's different, drop the command.
	// Also drop the command if we are using 1204, but there aren't enough parameters.
	if (has_chants)
	{
		if (params.size() < 5)
			return CMD_FAILURE;

		// Only proceed if the TSes are equal. If their TS is newer the channel
		// got recreated on their side, if it's older that means things are messed up,
		// because they haven't sent us an FJOIN earlier which could lower the chan TS.
		if (ConvToInt(params[1]) != c->age)
			return CMD_FAILURE;
	}

	// Now do things as usual but if required, apply an offset to the index when accessing params
	unsigned int indexoffset = (has_chants ? 1 : 0);
	time_t topicts = ConvToInt(params[1+indexoffset]);

	// See if the topic they sent is newer than ours (or we don't have a topic at all)
	if ((topicts >= c->topicset) || (c->topic.empty()))
	{
		if (c->topic != params[3+indexoffset])
		{
			// Update topic only when it differs from current topic
			c->topic.assign(params[3+indexoffset], 0, ServerInstance->Config->Limits.MaxTopic);
			c->WriteChannel(user, "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
		}

		// Always update setter and settime.
		c->setby.assign(params[2+indexoffset], 0, 127);
		c->topicset = topicts;
	}
	else
	{
		// We got a newer topic than this one, keep ours and drop the command
		return CMD_FAILURE;
	}

	// They haven't sent us a channel TS, add ours before passing the command on
	if (!has_chants)
	{
		parameterlist& p = const_cast<parameterlist&>(params);
		p.insert(p.begin()+1, ConvToStr(c->age));
	}

	return CMD_SUCCESS;
}

