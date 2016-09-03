/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

/** Handle /CHGNAME
 */
class CommandChgname : public Command
{
 public:
	CommandChgname(Module* Creator) : Command(Creator,"CHGNAME", 2, 2)
	{
		allow_empty_last_param = false;
		flags_needed = 'o';
		syntax = "<nick> <newname>";
		TRANSLATE2(TR_NICK, TR_TEXT);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);

		if ((!dest) || (dest->registered != REG_ALL))
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CMD_FAILURE;
		}

		if (parameters[1].empty())
		{
			user->WriteNotice("*** CHGNAME: GECOS must be specified");
			return CMD_FAILURE;
		}

		if (parameters[1].length() > ServerInstance->Config->Limits.MaxGecos)
		{
			user->WriteNotice("*** CHGNAME: GECOS too long");
			return CMD_FAILURE;
		}

		if (IS_LOCAL(dest))
		{
			dest->ChangeName(parameters[1]);
			ServerInstance->SNO->WriteGlobalSno('a', "%s used CHGNAME to change %s's GECOS to '%s'", user->nick.c_str(), dest->nick.c_str(), dest->fullname.c_str());
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class ModuleChgName : public Module
{
	CommandChgname cmd;

public:
	ModuleChgName() : cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for the CHGNAME command", VF_OPTCOMMON | VF_VENDOR);
	}
};

MODULE_INIT(ModuleChgName)
