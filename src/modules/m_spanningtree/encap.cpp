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
bool TreeSocket::Encap(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() > 1)
	{
		if (Instance->MatchText(Instance->Config->GetSID(), params[0]))
		{
			Event event((char*) &params, (Module*)this->Utils->Creator, "encap_received");
			event.Send(Instance);

			return true;
		}
		else
		{
			User* u = Instance->FindNick(params[0]);

			if (u && IS_LOCAL(u))
			{
				Event event((char*) &params, (Module*)this->Utils->Creator, "encap_received");
				event.Send(Instance);
			}

			return true;
		}

		if (params[0].find('*') != std::string::npos)
		{
			User* u = Instance->FindNick(params[0]);
			if (u)
				Utils->DoOneToAllButSender(prefix, "ENCAP", params, u->server);
			else
				Utils->DoOneToAllButSender(prefix, "ENCAP", params, params[0]);
		}
		else
			Utils->DoOneToOne(prefix, "ENCAP", params, prefix);
	}
	return true;
}

