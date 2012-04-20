/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

/** remote ADMIN. leet, huh? */
bool TreeSocket::Admin(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() > 0)
	{
		if (InspIRCd::Match(this->ServerInstance->Config->ServerName, params[0]))
		{
			/* It's for our server */
			string_list results;
			User* source = this->ServerInstance->FindNick(prefix);
			if (source)
			{
				std::deque<std::string> par;
				par.push_back(prefix);
				par.push_back("");
				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 256 "+source->nick+" :Administrative info for "+ServerInstance->Config->ServerName;
				Utils->DoOneToOne(this->ServerInstance->Config->GetSID(), "PUSH",par, source->server);
				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 257 "+source->nick+" :Name     - "+ServerInstance->Config->AdminName;
				Utils->DoOneToOne(this->ServerInstance->Config->GetSID(), "PUSH",par, source->server);
				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 258 "+source->nick+" :Nickname - "+ServerInstance->Config->AdminNick;
				Utils->DoOneToOne(this->ServerInstance->Config->GetSID(), "PUSH",par, source->server);
				par[1] = std::string("::")+ServerInstance->Config->ServerName+" 258 "+source->nick+" :E-Mail   - "+ServerInstance->Config->AdminEmail;
				Utils->DoOneToOne(this->ServerInstance->Config->GetSID(), "PUSH",par, source->server);
			}
		}
		else
		{
			/* Pass it on */
			User* source = this->ServerInstance->FindNick(prefix);
			if (source)
				Utils->DoOneToOne(prefix, "ADMIN", params, params[0]);
		}
	}
	return true;
}

