/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */



/** ENCAP */
void TreeSocket::Encap(User* who, parameterlist &params)
{
	if (params.size() > 1)
	{
		if (InspIRCd::Match(ServerInstance->Config->GetSID(), params[0]))
		{
			parameterlist plist(params.begin() + 2, params.end());
			ServerInstance->CallCommandHandler(params[1].c_str(), plist, who);
			// discard return value, ENCAP shall succeed even if the command does not exist
		}
		
		params[params.size() - 1] = ":" + params[params.size() - 1];

		if (params[0].find('*') != std::string::npos)
		{
			Utils->DoOneToAllButSender(who->server, "ENCAP", params, who->server);
		}
		else
			Utils->DoOneToOne(who->server, "ENCAP", params, params[0]);
	}
}

