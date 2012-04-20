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
	time_t ts = atoi(params[1].c_str());
	Channel* c = ServerInstance->FindChan(params[0]);
	if (c)
	{
		if ((ts >= c->topicset) || (c->topic.empty()))
		{
			if (c->topic != params[3])
			{
				// Update topic only when it differs from current topic
				c->topic.assign(params[3], 0, ServerInstance->Config->Limits.MaxTopic);
				c->WriteChannel(user, "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
			}

			// Always update setter and settime.
			c->setby.assign(params[2], 0, 127);
			c->topicset = ts;
		}
	}
	return CMD_SUCCESS;
}

