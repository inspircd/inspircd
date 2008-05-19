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
#include "commands/cmd_topic.h"


extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandTopic(Instance);
}

CmdResult CommandTopic::Handle (const std::vector<std::string>& parameters, User *user)
{
	Channel* Ptr;

	if (parameters.size() == 1)
	{
		Ptr = ServerInstance->FindChan(parameters[0]);
		if (Ptr)
		{
			if ((Ptr->IsModeSet('s')) && (!Ptr->HasUser(user)))
			{
				user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), Ptr->name.c_str());
				return CMD_FAILURE;
			}
			if (Ptr->topicset)
			{
				user->WriteNumeric(332, "%s %s :%s", user->nick.c_str(), Ptr->name.c_str(), Ptr->topic.c_str());
				user->WriteNumeric(333, "%s %s %s %lu", user->nick.c_str(), Ptr->name.c_str(), Ptr->setby.c_str(), (unsigned long)Ptr->topicset);
			}
			else
			{
				user->WriteNumeric(331, "%s %s :No topic is set.", user->nick.c_str(), Ptr->name.c_str());
			}
		}
		else
		{
			user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		return CMD_SUCCESS;
	}
	else if (parameters.size()>1)
	{
		Ptr = ServerInstance->FindChan(parameters[0]);
		if (Ptr)
		{
			if (IS_LOCAL(user))
			{
				if (!Ptr->HasUser(user))
				{
					user->WriteNumeric(442, "%s %s :You're not on that channel!",user->nick.c_str(), Ptr->name.c_str());
					return CMD_FAILURE;
				}
				if ((Ptr->IsModeSet('t')) && (Ptr->GetStatus(user) < STATUS_HOP))
				{
					user->WriteNumeric(482, "%s %s :You must be at least a half-operator to change the topic on this channel", user->nick.c_str(), Ptr->name.c_str());
					return CMD_FAILURE;
				}
			}

			char topic[MAXTOPIC];

			if (IS_LOCAL(user))
			{
				/* XXX: we need two string copies for a local topic, because we cant
				 * let a module see the topic as longer than it actually is
				 */
				int MOD_RESULT = 0;

				strlcpy(topic, parameters[1].c_str(), MAXTOPIC);
				FOREACH_RESULT(I_OnLocalTopicChange,OnLocalTopicChange(user,Ptr,topic));
				if (MOD_RESULT)
					return CMD_FAILURE;

				Ptr->topic.assign(topic, 0, MAXTOPIC);
			}
			else
			{
				/* Sneaky shortcut, one string copy for a remote topic */
				Ptr->topic.assign(parameters[1], 0, MAXTOPIC);
			}

			Ptr->setby.assign(ServerInstance->Config->FullHostInTopic ?
					user->GetFullHost() : user->nick,
					0, 128);

			Ptr->topicset = ServerInstance->Time();
			Ptr->WriteChannel(user, "TOPIC %s :%s", Ptr->name.c_str(), Ptr->topic.c_str());

			if (IS_LOCAL(user))
				/* We know 'topic' will contain valid data here */
				FOREACH_MOD(I_OnPostLocalTopicChange,OnPostLocalTopicChange(user, Ptr, topic));
		}
		else
		{
			user->WriteNumeric(401, "%s %s :No such nick/channel",user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
	}
	return CMD_SUCCESS;
}

