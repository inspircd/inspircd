/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
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

class CommandChgname final
	: public Command
{
public:
	CommandChgname(Module* Creator) : Command(Creator,"CHGNAME", 2, 2)
	{
		allow_empty_last_param = false;
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick> :<realname>" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		User* dest = ServerInstance->Users.Find(parameters[0]);

		if ((!dest) || (dest->registered != REG_ALL))
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
			ServerInstance->SNO.WriteGlobalSno('a', "%s used CHGNAME to change %s's real name to '%s\x0F'", user->nick.c_str(), dest->nick.c_str(), dest->GetRealName().c_str());
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
