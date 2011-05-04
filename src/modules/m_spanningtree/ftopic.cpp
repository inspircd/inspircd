/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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

