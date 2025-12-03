/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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
#include "core_channel.h"

CommandJoin::CommandJoin(Module* parent)
	: SplitCommand(parent, "JOIN", 1, 2)
{
	penalty = 2000;
	syntax = { "<channel>[,<channel>]+ [<key>[,<key>]+]" };
}

CmdResult CommandJoin::HandleLocal(LocalUser* user, const Params& parameters)
{
	if (parameters.size() > 1)
	{
		if (CommandParser::LoopCall(user, this, parameters, 0, 1, false))
			return CmdResult::SUCCESS;

		if (ServerInstance->Channels.IsChannel(parameters[0]))
		{
			Channel::JoinUser(user, parameters[0], false, parameters[1]);
			return CmdResult::SUCCESS;
		}
	}
	else
	{
		if (CommandParser::LoopCall(user, this, parameters, 0, -1, false))
			return CmdResult::SUCCESS;

		if (ServerInstance->Channels.IsChannel(parameters[0]))
		{
			Channel::JoinUser(user, parameters[0]);
			return CmdResult::SUCCESS;
		}
	}

	user->WriteNumeric(ERR_BADCHANMASK, parameters[0], "Invalid channel name");
	return CmdResult::FAILURE;
}
