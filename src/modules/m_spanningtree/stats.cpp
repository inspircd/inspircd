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
#include "link.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h */

bool TreeSocket::Stats(const std::string &prefix, parameterlist &params)
{
	/* Get the reply to a STATS query if it matches this servername,
	 * and send it back as a load of PUSH queries
	 */
	if (params.size() > 1)
	{
		if (InspIRCd::Match(ServerInstance->Config->ServerName, params[1]))
		{
			/* It's for our server */
			string_list results;
			User* source = ServerInstance->FindNick(prefix);
			if (source)
			{
				parameterlist par;
				par.push_back(prefix);
				par.push_back("");
				ServerInstance->DoStats(params[0][0], source, results);
				for (size_t i = 0; i < results.size(); i++)
				{
					par[1] = "::" + results[i];
					Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH",par, source->server);
				}
			}
		}
		else
		{
			/* Pass it on */
			User* source = ServerInstance->FindNick(prefix);
			if (source)
				Utils->DoOneToOne(source->uuid, "STATS", params, params[1]);
		}
	}
	return true;
}

