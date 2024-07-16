/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

#include "utils.h"
#include "treeserver.h"
#include "commands.h"

CmdResult CommandPing::Handle(User* user, Params& params)
{
	if (params[0] == ServerInstance->Config->ServerId)
	{
		// PING for us, reply with a PONG
		CmdBuilder reply("PONG");
		reply.push(user->uuid);
		if (params.size() >= 2)
			// If there is a second parameter, append it
			reply.push(params[1]);

		reply.Unicast(user);
	}
	return CmdResult::SUCCESS;
}
