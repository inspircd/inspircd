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

/** remote ADMIN. leet, huh? */
bool TreeSocket::Admin(const std::string &prefix, parameterlist &params)
{
	if (params.size() > 0)
	{
		if (InspIRCd::Match(ServerInstance->Config->ServerName, params[0]))
		{
			/* It's for our server */
			string_list results;
			User* source = ServerInstance->FindNick(prefix);
			if (source)
			{
				parameterlist par;
				par.push_back(prefix);
				par.push_back("");
				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 256 "+source->nick+" :Administrative info for "+ServerInstance->Config->ServerName;
				Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH",par, source->server);
				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 257 "+source->nick+" :Name     - "+ServerInstance->Config->AdminName;
				Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH",par, source->server);
				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 258 "+source->nick+" :Nickname - "+ServerInstance->Config->AdminNick;
				Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH",par, source->server);
				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 258 "+source->nick+" :E-Mail   - "+ServerInstance->Config->AdminEmail;
				Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH",par, source->server);
			}
		}
		else
		{
			/* Pass it on */
			User* source = ServerInstance->FindNick(prefix);
			if (source)
				Utils->DoOneToOne(prefix, "ADMIN", params, params[0]);
		}
	}
	return true;
}

