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
#include "commands.h"

CmdResult CommandAway::HandleRemote(::RemoteUser* u, std::vector<std::string>& params)
{
	if (params.size())
	{
		FOREACH_MOD(OnSetAway, (u, params.back()));

		if (params.size() > 1)
			u->awaytime = ConvToInt(params[0]);
		else
			u->awaytime = ServerInstance->Time();

		u->awaymsg = params.back();
	}
	else
	{
		FOREACH_MOD(OnSetAway, (u, ""));
		u->awaymsg.clear();
	}
	return CMD_SUCCESS;
}

CommandAway::Builder::Builder(User* user)
	: CmdBuilder(user, "AWAY")
{
	push_int(user->awaytime).push_last(user->awaymsg);
}

CommandAway::Builder::Builder(User* user, const std::string& msg)
	: CmdBuilder(user, "AWAY")
{
	if (!msg.empty())
		push_int(ServerInstance->Time()).push_last(msg);
}
