/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018, 2020, 2022, 2024, 2026 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
#include "core_user.h"

enum
{
	// From ircu.
	ERR_INVALIDUSERNAME = 468
};

CommandUser::CommandUser(Module* parent)
	: SplitCommand(parent, "USER", 4, 4)
{
	penalty = 0;
	syntax = { "<username> <unused> <unused> :<realname>" };
	works_before_reg = true;
}

CmdResult CommandUser::HandleLocal(LocalUser* user, const Params& parameters)
{
	/* A user may only send the USER command once */
	const auto& newreal = parameters[3];
	if (!(user->connected & User::CONN_USER))
	{
		// If the username is too long then we have to truncate it to mimic the
		// undocumented behaviour of other IRC servers. Failing to do so will
		// break clients.
		auto newuser = parameters[0];
		if (newuser.length() > ServerInstance->Config->Limits.MaxUser)
			newuser.erase(ServerInstance->Config->Limits.MaxUser);

		if (!ServerInstance->IsUser(newuser))
		{
			user->WriteNumeric(ERR_INVALIDUSERNAME, newuser, "Your username is not valid");
			return CmdResult::FAILURE;
		}
		else
		{
			user->ChangeRealUser(newuser, true);
			user->ChangeRealName(newreal);
			user->connected |= User::CONN_USER;
		}
	}
	else
	{
		user->WriteNumeric(ERR_ALREADYREGISTERED, "You may not resend the USER command");
		user->CommandFloodPenalty += 1000;
		return CmdResult::FAILURE;
	}

	/* parameters 2 and 3 are local and remote hosts, and are ignored */
	return CheckRegister(user);
}

CmdResult CommandUser::CheckRegister(LocalUser* user)
{
	// If the user is fully connected, return CmdResult::SUCCESS/CmdResult::FAILURE depending on
	// what modules say, otherwise just return CmdResult::SUCCESS without doing anything, knowing
	// the other handler will call us again
	if (user->connected == User::CONN_NICKUSER)
	{
		ModResult modres;
		FIRST_MOD_RESULT(OnUserRegister, modres, (user));
		if (modres == MOD_RES_DENY)
			return CmdResult::FAILURE;
	}

	return CmdResult::SUCCESS;
}
