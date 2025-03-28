/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020, 2022-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "core_oper.h"

namespace
{
	CmdResult FailedOper(LocalUser* user, const std::string& name)
	{
		user->WriteNumeric(ERR_NOOPERHOST, INSP_FORMAT("Failed to log into the \x02{}\x02 oper account (check the server log for details).", name));
		user->CommandFloodPenalty += 10'000;
		return CmdResult::FAILURE;
	}
}

CommandOper::CommandOper(Module* parent)
	: SplitCommand(parent, "OPER", 1, 2)
{
	syntax = { "<username> [<password>]" };
}

CmdResult CommandOper::HandleLocal(LocalUser* user, const Params& parameters)
{
	// Check whether the account exists.
	auto it = ServerInstance->Config->OperAccounts.find(parameters[0]);
	if (it == ServerInstance->Config->OperAccounts.end())
	{
		ServerInstance->SNO.WriteGlobalSno('o', "{} ({}) [{}] failed to log into the \x02{}\x02 oper account because no account with that name exists.",
			user->nick, user->GetRealUserHost(), user->GetAddress(), parameters[0]);
		return FailedOper(user, parameters[0]);
	}

	// Check whether the password is correct.
	auto account = it->second;
	const auto& password = parameters.size() > 1 ? parameters[1] : "";
	if (!account->CheckPassword(password))
	{
		ServerInstance->SNO.WriteGlobalSno('o', "{} ({}) [{}] failed to log into the \x02{}\x02 oper account because they specified the wrong password.",
			user->nick, user->GetRealUserHost(), user->GetAddress(), parameters[0]);
		return FailedOper(user, parameters[0]);
	}

	// Attempt to log the user into the account (modules will log if this fails).
	if (!user->OperLogin(account))
		return FailedOper(user, parameters[0]);

	// If they have reached this point then the login succeeded,
	return CmdResult::SUCCESS;
}
