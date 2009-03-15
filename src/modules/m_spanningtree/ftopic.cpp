/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
					c->WriteChannelWithServ(ServerInstance->Config->ServerName, "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
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

