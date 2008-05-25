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
			std::string oldtopic = c->topic;
			c->topic.assign(params[3], 0, Instance->Config->Limits.MaxTopic);
			c->setby.assign(params[2], 0, 127);
			c->topicset = ts;
			/* if the topic text is the same as the current topic,
			 * dont bother to send the TOPIC command out, just silently
			 * update the set time and set nick.
			 */
			if (oldtopic != params[3])
			{
				User* user = this->Instance->FindNick(source);
				if (!user)
				{
					c->WriteChannelWithServ(Instance->Config->ServerName, "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
				}
				else
				{
					c->WriteChannel(user, "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
					nsource = user->server;
				}
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

