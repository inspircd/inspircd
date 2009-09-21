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
#include "socket.h"
#include "xline.h"
#include "../transport.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::LocalPing(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;
	if (params.size() == 1)
	{
		std::string stufftobounce = params[0];
		this->WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" PONG "+stufftobounce);
		return true;
	}
	else
	{
		std::string forwardto = params[1];
		if (forwardto == ServerInstance->Config->ServerName || forwardto == ServerInstance->Config->GetSID())
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


