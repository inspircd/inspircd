/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
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
#include "account.h"

/* $ModDesc: Allow users to kill their own ghost sessions */

static dynamic_reference<AccountProvider> accounts("account");

/** Handle /GHOST
 */
class CommandGhost : public Command
{
 public:
	CommandGhost(Module* Creator) : Command(Creator,"GHOST", 1, 1)
	{
		syntax = "<nick>";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[0]);
		if(!target)
		{
			user->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel", user->nick.c_str(), parameters[0].c_str());
			return CMD_FAILURE;
		}
		if(IS_LOCAL(user))
		{
			if(!accounts || !accounts->IsRegistered(user))
			{
				user->WriteServ("NOTICE %s :You are not logged in", user->nick.c_str());
				return CMD_FAILURE;
			}
			if(accounts->GetAccountName(user) != accounts->GetAccountName(target))
			{
				user->WriteServ("NOTICE %s :They are not logged in as you", user->nick.c_str());
				return CMD_FAILURE;
			}
			if(user == target)
			{
				user->WriteServ("NOTICE %s :You may not ghost yourself", user->nick.c_str());
				return CMD_FAILURE;
			}
			user->WriteServ("NOTICE %s :User %s ghosted successfully", user->nick.c_str(), parameters[0].c_str());
		}
		if(IS_LOCAL(target))
			ServerInstance->Users->QuitUser(target, "GHOST command used by " + user->nick);
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

class ModuleGhost : public Module
{
	CommandGhost cmd_ghost;

 public:
	ModuleGhost() : cmd_ghost(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd_ghost);
	}

	Version GetVersion()
	{
		return Version("Allow users to kill their own ghost sessions", VF_VENDOR|VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleGhost)
