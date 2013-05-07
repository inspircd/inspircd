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

/** FTOPIC command */
CmdResult CommandFTopic::Handle(const std::vector<std::string>& params, User *user)
{
	Channel* c = ServerInstance->FindChan(params[0]);
	if (!c)
		return CMD_FAILURE;

	time_t ChanTS = ConvToInt(params[1]);
	if (!ChanTS)
		return CMD_INVALID;

	if (c->age < ChanTS)
		// Our channel TS is older, nothing to do
		return CMD_FAILURE;

	time_t ts = ConvToInt(params[2]);
	if (!ts)
		return CMD_INVALID;

	// Channel::topicset is initialized to 0 on channel creation, so their ts will always win if we never had a topic
	if (ts < c->topicset)
		return CMD_FAILURE;

	/*
	 * If the topics were updated at the exact same second, accept
	 * the remote only when it's "bigger" than ours as defined by
	 * string comparision, so non-empty topics always overridde
	 * empty topics if their timestamps are equal
	 */
	if ((ts == c->topicset) && (c->topic > params[4]))
		return CMD_FAILURE; // Topics were set at the exact same time, keep our topic and setter

	if (c->topic != params[4])
	{
		// Update topic only when it differs from current topic
		c->topic.assign(params[4], 0, ServerInstance->Config->Limits.MaxTopic);
		c->WriteChannel(user, "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
	}

	// Update setter and settime
	c->setby.assign(params[3], 0, 127);
	c->topicset = ts;

	return CMD_SUCCESS;
}

