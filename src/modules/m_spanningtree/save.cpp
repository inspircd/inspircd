/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "socket.h"
#include "xline.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

/* $ModDep: m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */

/**
 * SAVE command - force nick change to UID on timestamp match
 */
bool TreeSocket::ForceNick(const std::string &prefix, parameterlist &params)
{
	if (params.size() < 2)
		return true;

	User* u = ServerInstance->FindNick(params[0]);
	time_t ts = atol(params[1].c_str());

	if ((u) && (!IS_SERVER(u)) && (u->age == ts))
	{
		Utils->DoOneToAllButSender(prefix,"SAVE",params,prefix);

		if (!u->ForceNickChange(u->uuid.c_str()))
		{
			ServerInstance->Users->QuitUser(u, "Nickname collision");
		}
	}

	return true;
}

