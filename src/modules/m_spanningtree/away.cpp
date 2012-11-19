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

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"

bool TreeSocket::Away(const std::string &prefix, parameterlist &params)
{
	User* u = ServerInstance->FindNick(prefix);
	if ((!u) || (IS_SERVER(u)))
		return true;
	if (params.size())
	{
		FOREACH_MOD(I_OnSetAway, OnSetAway(u, params[params.size() - 1]));

		if (params.size() > 1)
			u->awaytime = atoi(params[0].c_str());
		else
			u->awaytime = ServerInstance->Time();

		u->awaymsg = params[params.size() - 1];

		params[params.size() - 1] = ":" + params[params.size() - 1];
	}
	else
	{
		FOREACH_MOD(I_OnSetAway, OnSetAway(u, ""));
		u->awaymsg.clear();
	}
	Utils->DoOneToAllButSender(prefix,"AWAY",params,u->server);
	return true;
}
