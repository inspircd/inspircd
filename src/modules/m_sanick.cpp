/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
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

class CommandSanick final
	: public Command
{
private:
	UserModeReference servprotectmode;

public:
	CommandSanick(Module* Creator)
		: Command(Creator, "SANICK", 2)
		, servprotectmode(Creator, "servprotect")
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick> <newnick>" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* target = ServerInstance->Users.Find(parameters[0], true);

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (target && target->IsModeSet(servprotectmode))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "Cannot use an SA command on a service");
				return CmdResult::FAILURE;
			}

			if (!target)
			{
				user->WriteNotice("*** No such nickname: '" + parameters[0] + "'");
				return CmdResult::FAILURE;
			}

			if (!ServerInstance->IsNick(parameters[1]))
			{
				user->WriteNotice("*** Invalid nickname: '" + parameters[1] + "'");
				return CmdResult::FAILURE;
			}
		}

		/* Have we hit target's server yet? */
		if (target && IS_LOCAL(target))
		{
			const std::string oldnick = target->nick;
			const std::string& newnick = parameters[1];
			if (!ServerInstance->Users.FindNick(newnick) && target->ChangeNick(newnick))
			{
				ServerInstance->SNO.WriteGlobalSno('a', user->nick + " used SANICK to change " + oldnick + " to " + newnick);
			}
			else
			{
				ServerInstance->SNO.WriteGlobalSno('a', user->nick + " failed SANICK (from " + oldnick + " to " + newnick + ")");
			}
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class ModuleSanick final
	: public Module
{
private:
	CommandSanick cmd;

public:
	ModuleSanick()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /SANICK command which allows server operators to change the nickname of a user.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleSanick)
