/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2016 Sadie Powell <sadie@witchery.services>
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

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDepends: core 3
/// $ModDesc: Allows services to forcibly oper a user.


#include "inspircd.h"

class CommandSVSOper : public Command
{
 public:
	CommandSVSOper(Module* Creator)
		: Command(Creator, "SVSOPER", 2, 2)
	{
		flags_needed = FLAG_SERVERONLY;
	}

	CmdResult Handle(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		if (!user->server->IsULine())
			return CMD_FAILURE;

		User* target = ServerInstance->FindUUID(parameters[0]);
		if (!target)
			return CMD_FAILURE;

		if (IS_LOCAL(target))
		{
			ServerConfig::OperIndex::iterator iter = ServerInstance->Config->OperTypes.find(parameters[1]);
			if (iter == ServerInstance->Config->OperTypes.end())
				return CMD_FAILURE;

			target->Oper(iter->second);
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) CXX11_OVERRIDE
	{
		User* target = ServerInstance->FindUUID(parameters[0]);
		if (!target)
			return ROUTE_LOCALONLY;
		return ROUTE_OPT_UCAST(target->server);
	}
};

class ModuleSVSOper : public Module
{
 private:
	CommandSVSOper command;

 public:
	ModuleSVSOper()
		: command(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows services to forcibly oper a user.", VF_OPTCOMMON);
	}
};

MODULE_INIT(ModuleSVSOper)
