/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Provides a spanning tree server link protocol */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

ModResult ModuleSpanningTree::HandleTime(const std::vector<std::string>& parameters, User* user)
{
	if ((IS_LOCAL(user)) && (parameters.size() > 0))
	{
		TreeServer* found = Utils->FindServerMask(parameters[0].c_str());
		if (found)
		{
			// we dont' override for local server
			if (found == Utils->TreeRoot)
				return MOD_RES_PASSTHRU;

			parameterlist params;
			params.push_back(found->GetName());
			params.push_back(user->uuid);
			Utils->DoOneToOne(ServerInstance->Config->GetSID(),"TIME",params,found->GetName());
		}
		else
		{
			user->WriteNumeric(ERR_NOSUCHSERVER, "%s %s :No such server",user->nick.c_str(),parameters[0].c_str());
		}
	}
	return MOD_RES_DENY;
}

