/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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
CmdResult CommandSave::Handle(User* user, Params& params)
{
	auto* u = ServerInstance->Users.FindUUID(params[0]);
	if (!u)
		return CmdResult::FAILURE;

	time_t ts = ServerCommand::ExtractTS(params[1]);
	if (u->nickchanged == ts)
		u->ChangeNick(u->uuid, SavedTimestamp);

	return CmdResult::SUCCESS;
}
