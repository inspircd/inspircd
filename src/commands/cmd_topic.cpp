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
#include "commands/cmd_topic.h"


extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandTopic(Instance);
}

CmdResult CommandTopic::Handle (const std::vector<std::string>& parameters, User *user)
{
	Channel* c;

	c = ServerInstance->FindChan(parameters[0]);
	if (!c)
	{
		user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
		return CMD_FAILURE;
	}

	if (parameters.size() == 1)
	{
		if (c)
		{
			if ((c->IsModeSet('s')) && (!c->HasUser(user)))
			{
				user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), c->name.c_str());
				return CMD_FAILURE;
			}

			if (c->topic.length())
			{
				user->WriteNumeric(332, "%s %s :%s", user->nick.c_str(), c->name.c_str(), c->topic.c_str());
				user->WriteNumeric(333, "%s %s %s %lu", user->nick.c_str(), c->name.c_str(), c->setby.c_str(), (unsigned long)c->topicset);
			}
			else
			{
				user->WriteNumeric(331, "%s %s :No topic is set.", user->nick.c_str(), c->name.c_str());
			}
		}
		return CMD_SUCCESS;
	}
	else if (parameters.size()>1)
	{
		std::string t = parameters[1]; // needed, in case a module wants to change it
		c->SetTopic(user, t);
	}

	return CMD_SUCCESS;
}

