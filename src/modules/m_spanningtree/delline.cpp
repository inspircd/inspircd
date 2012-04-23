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


bool TreeSocket::DelLine(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.size() < 2)
		return true;

	std::string setter = "<unknown>";

	User* user = ServerInstance->FindNick(prefix);
	if (user)
		setter = user->nick;
	else
	{
		TreeServer* t = Utils->FindServer(prefix);
		if (t)
			setter = t->GetName().c_str();
	}


	/* NOTE: No check needed on 'user', this function safely handles NULL */
	if (ServerInstance->XLines->DelLine(params[1].c_str(), params[0], user))
	{
		this->ServerInstance->SNO->WriteToSnoMask('x',"%s removed %s%s on %s.", setter.c_str(),
				params[0].c_str(), params[0].length() == 1 ? "LINE" : "", params[1].c_str());
		Utils->DoOneToAllButSender(prefix,"DELLINE", params, prefix);
	}
	return true;
}

