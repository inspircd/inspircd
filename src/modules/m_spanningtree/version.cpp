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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "xline.h"
#include "../transport.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

bool TreeSocket::ServerVersion(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 1)
		return true;

	TreeServer* ServerSource = Utils->FindServer(prefix);

	if (ServerSource)
	{
		ServerSource->SetVersion(params[0]);
	}
	params[0] = ":" + params[0];
	Utils->DoOneToAllButSender(prefix,"VERSION",params,prefix);
	return true;
}

