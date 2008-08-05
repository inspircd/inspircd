/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
	std::string nsource = source;
	Channel* c = this->Instance->FindChan(params[0]);
	if (c)
	{
		if ((ts >= c->topicset) || (c->topic.empty()))
		{
			User* user = this->Instance->FindNick(source);

			if (c->topic != params[3])
			{
				// Update topic only when it differs from current topic
				c->topic.assign(params[3], 0, Instance->Config->Limits.MaxTopic);
				if (!user)
				{
					c->WriteChannelWithServ(Instance->Config->ServerName, "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
				}
				else
				{
					c->WriteChannel(user, "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
				}
			}

			// Always update setter and settime.
			c->setby.assign(params[2], 0, 127);
			c->topicset = ts;

			/*
			 * Take careful note of what happens here;
			 * Above, we display the topic change to the server IF the topic incoming is different to the topic already set.
			 * HERE, we find the server the user that sent this topic is on, so we *do not* send topics back to the link they just
			 * came from. This *cannot* be easily merged with the above check!
			 *
			 * Thanks to Anope and Namegduf for finally helping me isolate this
			 *			-- w00t (5th/aug/2008)
			 */
			if (user)
			{
				nsource = user->server;
			}

			/* all done, send it on its way */
			params[3] = ":" + params[3];
			User* u = Instance->FindNick(nsource);
			if (u)
				Utils->DoOneToAllButSender(u->uuid,"FTOPIC",params,u->server);
			else
				Utils->DoOneToAllButSender(source,"FTOPIC",params,nsource);
		}

	}
	return true;
}

