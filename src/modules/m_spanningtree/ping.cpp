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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "transport.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::LocalPing(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 1)
		return true;
	if (params.size() == 1)
	{
		std::string stufftobounce = params[0];
		this->WriteLine(std::string(":")+this->ServerInstance->Config->GetSID()+" PONG "+stufftobounce);
		return true;
	}
	else
	{
		std::string forwardto = params[1];
		if (forwardto == this->ServerInstance->Config->ServerName || forwardto == this->ServerInstance->Config->GetSID())
		{
			// this is a ping for us, send back PONG to the requesting server
			params[1] = params[0];
			params[0] = forwardto;
			Utils->DoOneToOne(forwardto,"PONG",params,params[1]);
		}
		else
		{
			// not for us, pass it on :)
			Utils->DoOneToOne(prefix,"PING",params,forwardto);
		}
		return true;
	}
}


