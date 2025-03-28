/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
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
#include "numerichelper.h"

class CommandChgident final
	: public Command
{
public:
	CommandChgident(Module* Creator)
		: Command(Creator, "CHGIDENT", 2)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick> <username>" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* dest = ServerInstance->Users.Find(parameters[0], true);
		if (!dest)
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CmdResult::FAILURE;
		}

		if (parameters[1].length() > ServerInstance->Config->Limits.MaxUser)
		{
			user->WriteNotice("*** CHGIDENT: Username is too long");
			return CmdResult::FAILURE;
		}

		if (!ServerInstance->IsUser(parameters[1]))
		{
			user->WriteNotice("*** CHGIDENT: Invalid characters in username");
			return CmdResult::FAILURE;
		}

		if (IS_LOCAL(dest))
		{
			dest->ChangeDisplayedUser(parameters[1]);

			if (!user->server->IsService())
			{
				ServerInstance->SNO.WriteGlobalSno('a', "{} used CHGIDENT to change {}'s username to '{}'",
					user->nick, dest->nick, dest->GetDisplayedUser());
			}
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class ModuleChgIdent final
	: public Module
{
private:
	CommandChgident cmd;

public:
	ModuleChgIdent()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /CHGIDENT command which allows server operators to change the username of a user.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleChgIdent)
