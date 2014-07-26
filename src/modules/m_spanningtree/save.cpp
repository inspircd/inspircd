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

#include "utils.h"
#include "treesocket.h"
#include "commands.h"

/**
 * SAVE command - force nick change to UID on timestamp match
 */
CmdResult CommandSave::Handle(User* user, std::vector<std::string>& params)
{
	User* u = ServerInstance->FindUUID(params[0]);
	if ((!u) || (IS_SERVER(u)))
		return CMD_FAILURE;

	time_t ts = atol(params[1].c_str());

	if (u->age == ts)
		u->ChangeNick(u->uuid, SavedTimestamp);

	return CMD_SUCCESS;
}
