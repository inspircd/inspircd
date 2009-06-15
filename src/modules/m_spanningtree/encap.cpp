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

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */



/** remote MOTD. leet, huh? */
bool TreeSocket::Encap(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() > 1)
	{
		if (InspIRCd::Match(ServerInstance->Config->GetSID(), params[0]))
		{
			Event event((char*) &params, (Module*)this->Utils->Creator, "encap_received");
			event.Send(ServerInstance);
		}
		
		params[params.size() - 1] = ":" + params[params.size() - 1];

		if (params[0].find('*') != std::string::npos)
		{
			Utils->DoOneToAllButSender(prefix, "ENCAP", params, prefix);
		}
		else
			Utils->DoOneToOne(prefix, "ENCAP", params, params[0]);
	}
	return true;
}

