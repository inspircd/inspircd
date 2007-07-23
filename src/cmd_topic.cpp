/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "modules.h"
#include "commands/cmd_topic.h"


extern "C" DllExport command_t* init_command(InspIRCd* Instance)
{
	return new cmd_topic(Instance);
}

CmdResult cmd_topic::Handle (const char** parameters, int pcnt, userrec *user)
{
	chanrec* Ptr;

	if (pcnt == 1)
	{
		Ptr = ServerInstance->FindChan(parameters[0]);
		if (Ptr)
		{
			if ((Ptr->IsModeSet('s')) && (!Ptr->HasUser(user)))
			{
				user->WriteServ("401 %s %s :No such nick/channel",user->nick, Ptr->name);
				return CMD_FAILURE;
			}
			if (Ptr->topicset)
			{
				user->WriteServ("332 %s %s :%s", user->nick, Ptr->name, Ptr->topic);
				user->WriteServ("333 %s %s %s %d", user->nick, Ptr->name, Ptr->setby, Ptr->topicset);
			}
			else
			{
				user->WriteServ("331 %s %s :No topic is set.", user->nick, Ptr->name);
			}
		}
		else
		{
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
			return CMD_FAILURE;
		}
		return CMD_SUCCESS;
	}
	else if (pcnt>1)
	{
		Ptr = ServerInstance->FindChan(parameters[0]);
		if (Ptr)
		{
			if (IS_LOCAL(user))
			{
				if (!Ptr->HasUser(user))
				{
					user->WriteServ("442 %s %s :You're not on that channel!",user->nick, Ptr->name);
					return CMD_FAILURE;
				}
				if ((Ptr->IsModeSet('t')) && (Ptr->GetStatus(user) < STATUS_HOP))
				{
					user->WriteServ("482 %s %s :You must be at least a half-operator to change the topic on this channel", user->nick, Ptr->name);
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

				strlcpy(topic,parameters[1],MAXTOPIC-1);
				FOREACH_RESULT(I_OnLocalTopicChange,OnLocalTopicChange(user,Ptr,topic));
				if (MOD_RESULT)
					return CMD_FAILURE;

				strlcpy(Ptr->topic,topic,MAXTOPIC-1);
			}
			else
			{
				/* Sneaky shortcut, one string copy for a remote topic */
				strlcpy(Ptr->topic, parameters[1], MAXTOPIC-1);
			}

			if (ServerInstance->Config->FullHostInTopic)
				strlcpy(Ptr->setby,user->GetFullHost(),127);
			else
				strlcpy(Ptr->setby,user->nick,127);

			Ptr->topicset = ServerInstance->Time();
			Ptr->WriteChannel(user, "TOPIC %s :%s", Ptr->name, Ptr->topic);

			if (IS_LOCAL(user))
				/* We know 'topic' will contain valid data here */
				FOREACH_MOD(I_OnPostLocalTopicChange,OnPostLocalTopicChange(user, Ptr, topic));
		}
		else
		{
			user->WriteServ("401 %s %s :No such nick/channel",user->nick, parameters[0]);
			return CMD_FAILURE;
		}
	}
	return CMD_SUCCESS;
}

