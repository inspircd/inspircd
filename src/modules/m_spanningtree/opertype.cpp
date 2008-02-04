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

/** Because the core won't let users or even SERVERS set +o,
 * we use the OPERTYPE command to do this.
 */
bool TreeSocket::OperType(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() != 1)
		return true;
	std::string opertype = params[0];
	User* u = this->Instance->FindNick(prefix);
	if (u)
	{
		if (!u->IsModeSet('o'))
			this->Instance->Users->all_opers.push_back(u);
		u->modes[UM_OPERATOR] = 1;
		strlcpy(u->oper,opertype.c_str(),NICKMAX-1);
		Utils->DoOneToAllButSender(u->nick,"OPERTYPE",params,u->server);

		TreeServer* remoteserver = Utils->FindServer(u->server);
		bool dosend = true;

		if (this->Utils->quiet_bursts)
		{
			/*
			 * If quiet bursts are enabled, and server is bursting or silent uline (i.e. services),
			 * then do nothing. -- w00t
			 */
			if (
				remoteserver->bursting ||
				this->Instance->SilentULine(this->Instance->FindServerNamePtr(u->server))
			   )
			{
				dosend = false;
			}
		}

		if (dosend)
			this->Instance->SNO->WriteToSnoMask('o',"From %s: User %s (%s@%s) is now an IRC operator of type %s",u->server, u->nick,u->ident,u->host,irc::Spacify(opertype.c_str()));
	}
	return true;
}

