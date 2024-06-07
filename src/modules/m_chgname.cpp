/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2012, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

class CommandChgname final
	: public Command
{
public:
	CommandChgname(Module* Creator)
		: Command(Creator, "CHGNAME", 2, 2)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick> :<realname>" };
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

		if (parameters[1].empty())
		{
			user->WriteNotice("*** CHGNAME: Real name must be specified");
			return CmdResult::FAILURE;
		}

		if (parameters[1].length() > ServerInstance->Config->Limits.MaxReal)
		{
			user->WriteNotice("*** CHGNAME: Real name is too long");
			return CmdResult::FAILURE;
		}

		if (IS_LOCAL(dest))
		{
			dest->ChangeRealName(parameters[1]);
			ServerInstance->SNO.WriteGlobalSno('a', "{} used CHGNAME to change {}'s real name to '{}\x0F'", user->nick, dest->nick, dest->GetRealName());
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class ModuleChgName final
	: public Module
{
private:
	CommandChgname cmd;

public:
	ModuleChgName()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /CHGNAME command which allows server operators to change the real name of a user.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleChgName)
