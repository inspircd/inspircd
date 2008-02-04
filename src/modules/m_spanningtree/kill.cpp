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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

bool TreeSocket::RemoteKill(const std::string &prefix, std::deque<std::string> &params)
{ 	 
	if (params.size() != 2)
		return true;

	User* who = this->Instance->FindNick(params[0]);

	if (who)
	{
		/* Prepend kill source, if we don't have one */ 	 
		if (*(params[1].c_str()) != '[')
		{
			params[1] = "[" + prefix + "] Killed (" + params[1] +")";
		}
		std::string reason = params[1];
		params[1] = ":" + params[1];
		Utils->DoOneToAllButSender(prefix,"KILL",params,prefix);
		// NOTE: This is safe with kill hiding on, as RemoteKill is only reached if we have a server prefix.
		// in short this is not executed for USERS.
		who->Write(":%s KILL %s :%s (%s)", prefix.c_str(), who->nick, prefix.c_str(), reason.c_str());
		User::QuitUser(this->Instance,who,reason);
	}
	return true;
}

