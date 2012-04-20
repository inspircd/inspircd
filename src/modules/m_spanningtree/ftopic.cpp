/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


/** FTOPIC command */
bool TreeSocket::ForceTopic(const std::string &source, std::deque<std::string> &params)
{
	if (params.size() != 4)
		return true;
	time_t ts = atoi(params[1].c_str());
	Channel* c = this->ServerInstance->FindChan(params[0]);
	if (c)
	{
		if ((ts >= c->topicset) || (c->topic.empty()))
		{
			if (c->topic != params[3])
			{
				User* user = this->ServerInstance->FindNick(source);
				// Update topic only when it differs from current topic
				c->topic.assign(params[3], 0, ServerInstance->Config->Limits.MaxTopic);
				if (!user)
				{
					std::string sourceserv = Utils->FindServer(source)->GetName();
					c->WriteChannelWithServ(sourceserv.c_str(), "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
				}
				else
				{
					c->WriteChannel(user, "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
				}
			}

			// Always update setter and settime.
			c->setby.assign(params[2], 0, 127);
			c->topicset = ts;

			/* all done, send it on its way */
			params[3] = ":" + params[3];
			Utils->DoOneToAllButSender(source,"FTOPIC",params,source);
		}

	}
	return true;
}

