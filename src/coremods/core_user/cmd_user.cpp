/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

CommandUser::CommandUser(Module* parent)
	: SplitCommand(parent, "USER", 4, 4)
{
	works_before_reg = true;
	Penalty = 0;
	syntax = "<username> <localhost> <remotehost> <GECOS>";
}

CmdResult CommandUser::HandleLocal(const std::vector<std::string>& parameters, LocalUser *user)
{
	/* A user may only send the USER command once */
	if (!(user->registered & REG_USER))
	{
		if (!ServerInstance->IsIdent(parameters[0]))
		{
			/*
			 * RFC says we must use this numeric, so we do. Let's make it a little more nub friendly though. :)
			 *  -- Craig, and then w00t.
			 */
			user->WriteNumeric(ERR_NEEDMOREPARAMS, "USER :Your username is not valid");
			return CMD_FAILURE;
		}
		else
		{
			/*
			 * The ident field is IDENTMAX+2 in size to account for +1 for the optional
			 * ~ character, and +1 for null termination, therefore we can safely use up to
			 * IDENTMAX here.
			 */
			user->ChangeIdent(parameters[0]);
			user->fullname.assign(parameters[3].empty() ? "No info" : parameters[3], 0, ServerInstance->Config->Limits.MaxGecos);
			user->registered = (user->registered | REG_USER);
		}
	}
	else
	{
		user->WriteNumeric(ERR_ALREADYREGISTERED, ":You may not reregister");
		user->CommandFloodPenalty += 1000;
		return CMD_FAILURE;
	}

	/* parameters 2 and 3 are local and remote hosts, and are ignored */
	return CheckRegister(user);
}

CmdResult CommandUser::CheckRegister(LocalUser* user)
{
	// If the user is registered, return CMD_SUCCESS/CMD_FAILURE depending on what modules say, otherwise just
	// return CMD_SUCCESS without doing anything, knowing the other handler will call us again
	if (user->registered == REG_NICKUSER)
	{
		ModResult MOD_RESULT;
		FIRST_MOD_RESULT(OnUserRegister, MOD_RESULT, (user));
		if (MOD_RESULT == MOD_RES_DENY)
			return CMD_FAILURE;
	}

	return CMD_SUCCESS;
}
