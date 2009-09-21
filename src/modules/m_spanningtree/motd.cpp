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



/** remote MOTD. leet, huh? */
bool TreeSocket::Motd(const std::string &prefix, parameterlist &params)
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

				if (!ServerInstance->Config->MOTD.size())
				{
					par[1] = std::string("::")+ServerInstance->Config->ServerName+" 422 "+source->nick+" :Message of the day file is missing.";
					Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH",par, source->server);
					return true;
				}

				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 375 "+source->nick+" :"+ServerInstance->Config->ServerName+" message of the day";
				Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH",par, source->server);

				for (unsigned int i = 0; i < ServerInstance->Config->MOTD.size(); i++)
				{
					par[1] = std::string("::")+ServerInstance->Config->ServerName+" 372 "+source->nick+" :- "+ServerInstance->Config->MOTD[i];
					Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH",par, source->server);
				}

				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 376 "+source->nick+" :End of message of the day.";
				Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH",par, source->server);
			}
		}
		else
		{
			/* Pass it on */
			User* source = ServerInstance->FindNick(prefix);
			if (source)
				Utils->DoOneToOne(prefix, "MOTD", params, params[0]);
		}
	}
	return true;
}

