/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
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



/** remote MOTD. leet, huh? */
bool TreeSocket::ServerMessage(const std::string &messagetype, const std::string &prefix, std::deque<std::string> &params, const std::string &sourceserv)
{
	if (params.size() >= 2)
	{
		CUList except_list;
		char status = '\0';
		const char* target = params[0].c_str();
		std::string text = params[1].c_str();

		if (ServerInstance->Modes->FindPrefix(*target))
		{
			status = *target;
			target++;
		}

		Channel* channel = ServerInstance->FindChan(target);

		if (channel)
		{
			if (messagetype == "PRIVMSG")
			{
				FOREACH_MOD_I(ServerInstance, I_OnUserMessage, OnUserMessage(NULL, channel, TYPE_CHANNEL, text, status, except_list));
			}
			else
			{
				FOREACH_MOD_I(ServerInstance, I_OnUserNotice, OnUserNotice(NULL, channel, TYPE_CHANNEL, text, status, except_list));
			}
			TreeServer* s = Utils->FindServer(prefix);
			if (s)
			{
				FOREACH_MOD_I(ServerInstance, I_OnText, OnText(NULL, channel, TYPE_CHANNEL, text, status, except_list));
				channel->WriteChannelWithServ(s->GetName().c_str(), "%s %s :%s", messagetype.c_str(), channel->name.c_str(), text.c_str());
			}
		}
		else
		{
			User* user = ServerInstance->FindNick(target);

			if (user)
			{
				if (messagetype == "PRIVMSG")
				{
					FOREACH_MOD_I(ServerInstance, I_OnUserMessage, OnUserMessage(NULL, user, TYPE_USER, text, 0, except_list));
				}
				else
				{
					FOREACH_MOD_I(ServerInstance, I_OnUserNotice, OnUserNotice(NULL, user, TYPE_USER, text, 0, except_list));
				}
				TreeServer* s = Utils->FindServer(prefix);
				if (s)
				{
					FOREACH_MOD_I(ServerInstance, I_OnText, OnText(NULL, user, TYPE_USER, text, status, except_list));
					user->Write(":%s %s %s :%s", s->GetName().c_str(), messagetype.c_str(), user->nick.c_str(), text.c_str());
				}

			}
		}

		/* Propogate as channel privmsg */
		return Utils->DoOneToAllButSenderRaw(":" + prefix + " " + messagetype + " " + target + " :" + text, sourceserv, prefix, assign(messagetype), params);
	}
	return true;
}

