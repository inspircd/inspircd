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

/* $ModDesc: Provides support for the CHGNAME command */

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
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);

		if ((!dest) || (dest->registered != REG_ALL))
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}

		if (parameters[1].empty())
		{
			user->WriteServ("NOTICE %s :*** CHGNAME: GECOS must be specified", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (parameters[1].length() > ServerInstance->Config->Limits.MaxGecos)
		{
			user->WriteServ("NOTICE %s :*** CHGNAME: GECOS too long", user->nick.c_str());
			return CMD_FAILURE;
		}

		if (IS_LOCAL(dest))
		{
			dest->ChangeName(parameters[1].c_str());
			ServerInstance->SNO->WriteGlobalSno('a', "%s used CHGNAME to change %s's GECOS to '%s'", user->nick.c_str(), dest->nick.c_str(), dest->fullname.c_str());
		}

		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};


class ModuleChgName : public Module
{
	CommandChgname cmd;

public:
	ModuleChgName() : cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	virtual ~ModuleChgName()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for the CHGNAME command", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(ModuleChgName)
