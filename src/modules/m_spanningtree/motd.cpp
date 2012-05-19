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



/** remote MOTD. leet, huh? */
bool TreeSocket::Motd(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() > 0)
	{
		if (InspIRCd::Match(this->ServerInstance->Config->ServerName, params[0]))
		{
			/* It's for our server */
			User* source = this->ServerInstance->FindNick(prefix);

			if (source)
			{
				if (!ServerInstance->Config->MOTD.size())
				{
					ServerInstance->PI->PushToClient(source, std::string("::")+ServerInstance->Config->ServerName+" 422 "+source->nick+" :Message of the day file is missing.");
					return true;
				}

				ServerInstance->PI->PushToClient(source, std::string("::")+ServerInstance->Config->ServerName+" 375 "+source->nick+" :"+ServerInstance->Config->ServerName+" message of the day");

				std::string lineprefix = std::string("::") + ServerInstance->Config->ServerName + " 372 " + source->nick + " :- ";
				for (file_cache::const_iterator i = ServerInstance->Config->MOTD.begin(); i != ServerInstance->Config->MOTD.end(); ++i)
				{
					ServerInstance->PI->PushToClient(source, lineprefix + *i);
				}
				ServerInstance->PI->PushToClient(source, std::string("::")+ServerInstance->Config->ServerName+" 376 "+source->nick+" :End of message of the day.");
			}
		}
		else
		{
			/* Pass it on */
			User* source = this->ServerInstance->FindNick(prefix);
			if (source)
				Utils->DoOneToOne(prefix, "MOTD", params, params[0]);
		}
	}
	return true;
}
