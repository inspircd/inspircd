/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "commands/cmd_pass.h"

extern "C" DllExport Command* init_command(InspIRCd* Instance)
{
	return new CommandPass(Instance);
}

CmdResult CommandPass::Handle (const std::vector<std::string>& parameters, User *user)
{
	// Check to make sure they havnt registered -- Fix by FCS
	if (user->registered == REG_ALL)
	{
		user->WriteNumeric(ERR_ALREADYREGISTERED, "%s :You may not reregister",user->nick.c_str());
		return CMD_FAILURE;
	}
	ConnectClass* a = user->GetClass();
	if (!a)
		return CMD_FAILURE;

	user->password.assign(parameters[0], 0, 63);
	if (!ServerInstance->PassCompare(user, a->pass.c_str(), parameters[0].c_str(), a->hash.c_str()))
		user->haspassed = true;

	return CMD_SUCCESS;
}
