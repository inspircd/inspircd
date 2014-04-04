/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Provides support for an SAQUIT command, exits user with a reason */

/** Handle /SAQUIT
 */
class CommandSaquit : public Command
{
 public:
	CommandSaquit(Module* Creator) : Command(Creator, "SAQUIT", 2, 2)
	{
		flags_needed = 'o'; Penalty = 0; syntax = "<nick> <reason>";
		TRANSLATE3(TR_NICK, TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if ((dest) && (!IS_SERVER(dest)) && (dest->registered == REG_ALL))
		{
			if (ServerInstance->ULine(dest->server))
			{
				user->WriteNumeric(ERR_NOPRIVILEGES, "%s :Cannot use an SA command on a u-lined client",user->nick.c_str());
				return CMD_FAILURE;
			}

			// Pass the command on, so the client's server can quit it properly.
			if (!IS_LOCAL(dest))
				return CMD_SUCCESS;
			
			ServerInstance->SNO->WriteGlobalSno('a', user->nick+" used SAQUIT to make "+dest->nick+" quit with a reason of "+parameters[1]);

			ServerInstance->Users->QuitUser(dest, parameters[1]);
			return CMD_SUCCESS;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid nickname '%s'", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		User* dest = ServerInstance->FindNick(parameters[0]);
		if (dest)
			return ROUTE_OPT_UCAST(dest->server);
		return ROUTE_LOCALONLY;
	}
};

class ModuleSaquit : public Module
{
	CommandSaquit cmd;
 public:
	ModuleSaquit()
		: cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	virtual ~ModuleSaquit()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for an SAQUIT command, exits user with a reason", VF_OPTCOMMON | VF_VENDOR);
	}

};

MODULE_INIT(ModuleSaquit)
