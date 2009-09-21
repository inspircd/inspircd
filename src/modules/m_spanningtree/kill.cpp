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

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */



bool TreeSocket::RemoteKill(const std::string &prefix, parameterlist &params)
{
	if (params.size() != 2)
		return true;

	User* who = ServerInstance->FindNick(params[0]);

	if (who)
	{
		/* Prepend kill source, if we don't have one */
		if (*(params[1].c_str()) != 'K')
		{
			params[1] = "Killed (" + params[1] +")";
		}
		std::string reason = params[1];
		params[1] = ":" + params[1];
		Utils->DoOneToAllButSender(prefix,"KILL",params,prefix);
		TreeServer* src = Utils->FindServer(prefix);
		if (src)
		{
			// this shouldn't ever be null, but it doesn't hurt to check
			who->Write(":%s KILL %s :%s (%s)", src->GetName().c_str(), who->nick.c_str(), src->GetName().c_str(), reason.c_str());
		}
		ServerInstance->Users->QuitUser(who, reason);
	}
	return true;
}

