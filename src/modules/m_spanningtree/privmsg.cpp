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

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/** remote server PRIVMSG/NOTICE */
bool TreeSocket::ServerMessage(const std::string &messagetype, const std::string &prefix, parameterlist &params, const std::string &sourceserv)
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
				FOREACH_MOD_I(ServerInstance, I_OnUserMessage, OnUserMessage(Utils->ServerUser, channel, TYPE_CHANNEL, text, status, except_list));
			}
			else
			{
				FOREACH_MOD_I(ServerInstance, I_OnUserNotice, OnUserNotice(Utils->ServerUser, channel, TYPE_CHANNEL, text, status, except_list));
			}
			TreeServer* s = Utils->FindServer(prefix);
			if (s)
			{
				FOREACH_MOD_I(ServerInstance, I_OnText, OnText(Utils->ServerUser, channel, TYPE_CHANNEL, text, status, except_list));
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
					FOREACH_MOD_I(ServerInstance, I_OnUserMessage, OnUserMessage(Utils->ServerUser, user, TYPE_USER, text, 0, except_list));
				}
				else
				{
					FOREACH_MOD_I(ServerInstance, I_OnUserNotice, OnUserNotice(Utils->ServerUser, user, TYPE_USER, text, 0, except_list));
				}
				TreeServer* s = Utils->FindServer(prefix);
				if (s)
				{
					FOREACH_MOD_I(ServerInstance, I_OnText, OnText(Utils->ServerUser, user, TYPE_USER, text, status, except_list));
					user->Write(":%s %s %s :%s", s->GetName().c_str(), messagetype.c_str(), user->nick.c_str(), text.c_str());
				}

			}
		}

		/* Propogate as channel privmsg */
		return Utils->DoOneToAllButSenderRaw(":" + prefix + " " + messagetype + " " + target + " :" + text, sourceserv, prefix, assign(messagetype), params);
	}
	return true;
}

