/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
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

/** FTOPIC command */
CmdResult CommandFTopic::Handle(User* user, Params& params)
{
	auto* c = ServerInstance->Channels.Find(params[0]);
	if (!c)
		return CmdResult::FAILURE;

	if (c->age < ServerCommand::ExtractTS(params[1]))
		// Our channel TS is older, nothing to do
		return CmdResult::FAILURE;

	// Channel::topicset is initialized to 0 on channel creation, so their ts will always win if we never had a topic
	time_t ts = ServerCommand::ExtractTS(params[2]);
	if (ts < c->topicset)
		return CmdResult::FAILURE;

	// The topic text is always the last parameter
	const std::string& newtopic = params.back();

	// If there is a setter in the message use that, otherwise use the message source
	const std::string& setter = ((params.size() > 4) ? params[3] : (ServerInstance->Config->MaskInTopic ? user->GetMask() : user->nick));

	/*
	 * If the topics were updated at the exact same second, accept
	 * the remote only when it's "bigger" than ours as defined by
	 * string comparison, so non-empty topics always override
	 * empty topics if their timestamps are equal
	 *
	 * Similarly, if the topic texts are equal too, keep one topic
	 * setter and discard the other
	 */
	if (ts == c->topicset)
	{
		// Discard if their topic text is "smaller"
		if (c->topic > newtopic)
			return CmdResult::FAILURE;

		// If the texts are equal in addition to the timestamps, decide which setter to keep
		if ((c->topic == newtopic) && (c->setby >= setter))
			return CmdResult::FAILURE;
	}

	c->SetTopic(user, newtopic, ts, &setter);
	return CmdResult::SUCCESS;
}

// Used when bursting and in reply to RESYNC, contains topic setter as the 4th parameter
CommandFTopic::Builder::Builder(Channel* chan)
	: CmdBuilder("FTOPIC")
{
	push(chan->name);
	push_int(chan->age);
	push_int(chan->topicset);
	push(chan->setby);
	push_last(chan->topic);
}

// Used when changing the topic, the setter is the message source
CommandFTopic::Builder::Builder(User* user, Channel* chan)
	: CmdBuilder(user, "FTOPIC")
{
	push(chan->name);
	push_int(chan->age);
	push_int(chan->topicset);
	push_last(chan->topic);
}
