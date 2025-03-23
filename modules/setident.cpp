/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

class CommandSetident final
	: public Command
{
public:
	CommandSetident(Module* Creator)
		: Command(Creator, "SETIDENT", 1)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<username>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (parameters[0].size() > ServerInstance->Config->Limits.MaxUser)
		{
			user->WriteNotice("*** SETIDENT: Username is too long");
			return CmdResult::FAILURE;
		}

		if (!ServerInstance->Users.IsUser(parameters[0]))
		{
			user->WriteNotice("*** SETIDENT: Invalid characters in username");
			return CmdResult::FAILURE;
		}

		user->ChangeDisplayedUser(parameters[0]);
		ServerInstance->SNO.WriteGlobalSno('a', "{} used SETIDENT to change their username to '{}'", user->nick, user->GetRealUser());

		return CmdResult::SUCCESS;
	}
};

class ModuleSetIdent final
	: public Module
{
private:
	CommandSetident cmd;

public:
	ModuleSetIdent()
		: Module(VF_VENDOR, "Adds the /SETIDENT command which allows server operators to change their username.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleSetIdent)
