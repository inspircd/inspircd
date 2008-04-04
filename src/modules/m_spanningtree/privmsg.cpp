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



/** remote MOTD. leet, huh? */
bool TreeSocket::ServerMessage(const std::string &messagetype, const std::string &prefix, std::deque<std::string> &params, const std::string &sourceserv)
{
	if (params.size() >= 2)
	{
		CUList except_list;
		char status = '\0';
		const char* target = params[0].c_str();
		std::string text = params[1].c_str();

		if ((*target == '@') || (*target == '%') || (*target == '+'))
		{
			status = *target;
			target++;
		}

		Channel* channel = Instance->FindChan(target);
		
		if (target)
		{
			if (messagetype == "PRIVMSG")
			{
				FOREACH_MOD_I(Instance, I_OnUserMessage, OnUserMessage(NULL, channel, TYPE_SERVER, text, status, except_list));
			}
			else
			{
				FOREACH_MOD_I(Instance, I_OnUserNotice, OnUserNotice(NULL, channel, TYPE_SERVER, text, status, except_list));
			}
			TreeServer* s = Utils->FindServer(prefix);
			if (s)
			{
				FOREACH_MOD_I(Instance, I_OnText, OnText(NULL, channel, TYPE_SERVER, text, status, except_list));
				channel->WriteChannelWithServ(s->GetName().c_str(), "%s %s :%s", messagetype.c_str(), channel->name, text.c_str());
			}
		}

		/* Propogate as channel privmsg */
		return Utils->DoOneToAllButSenderRaw(":" + prefix + " " + messagetype + " " + channel->name + " :" + text, sourceserv, prefix, assign(messagetype), params);
	}
	return true;
}

