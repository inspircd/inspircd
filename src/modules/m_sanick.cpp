/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2005 Craig Edwards <craigedwards@brainbox.cc>
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

/** Handle /SANICK
 */
class CommandSanick : public Command
{
 public:
	CommandSanick(Module* Creator) : Command(Creator,"SANICK", 2)
	{
		allow_empty_last_param = false;
		flags_needed = 'o'; syntax = "<nick> <new-nick>";
		TRANSLATE2(TR_NICK, TR_TEXT);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);

		/* Do local sanity checks and bails */
		if (IS_LOCAL(user))
		{
			if (target && target->server->IsULine())
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "Cannot use an SA command on a u-lined client");
				return CMD_FAILURE;
			}

			if ((!target) || (target->registered != REG_ALL))
			{
				user->WriteNotice("*** No such nickname: '" + parameters[0] + "'");
				return CMD_FAILURE;
			}

			if (!ServerInstance->IsNick(parameters[1]))
			{
				user->WriteNotice("*** Invalid nickname '" + parameters[1] + "'");
				return CMD_FAILURE;
			}
		}

		/* Have we hit target's server yet? */
		if (target && IS_LOCAL(target))
		{
			std::string oldnick = user->nick;
			std::string newnick = target->nick;
			if (target->ChangeNick(parameters[1]))
			{
				ServerInstance->SNO->WriteGlobalSno('a', oldnick+" used SANICK to change "+newnick+" to "+parameters[1]);
			}
			else
			{
				ServerInstance->SNO->WriteGlobalSno('a', oldnick+" failed SANICK (from "+newnick+" to "+parameters[1]+")");
			}
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};


class ModuleSanick : public Module
{
	CommandSanick cmd;
 public:
	ModuleSanick()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for SANICK command", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleSanick)
