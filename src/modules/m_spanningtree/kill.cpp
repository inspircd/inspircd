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



bool TreeSocket::RemoteKill(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() != 2)
		return true;

	User* who = this->ServerInstance->FindNick(params[0]);

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
		this->ServerInstance->Users->QuitUser(who, reason);
	}
	return true;
}

