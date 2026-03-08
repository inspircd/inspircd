/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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
#include "modules/ircv3.h"
#include "numerichelper.h"

class CommandSetIdent final
	: public Command
{
public:
	IRCv3::ReplyCapReference cap;

	CommandSetIdent(Module* mod)
		: Command(mod, "SETIDENT", 1, 2)
		, cap(mod)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "[<nick>] :<newuser>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* target = user;
		if (parameters.size() > 1)
		{
			const auto& targetnick = parameters[0];
			if (user->IsLocal())
			{
				// For local users we need to check if they can use the command.
				target = ServerInstance->Users.FindNick(targetnick, true);
				if (user != target && !user->HasPrivPermission("users/setident-others"))
				{
					user->WriteNumeric(Numerics::NoPrivileges("your server operator account does not have the users/setident-others privilege"));
					return CmdResult::FAILURE;
				}
			}
			else
			{
				// For remote users their server will have checked privs.
				target = ServerInstance->Users.Find(targetnick);
			}

			if (!target)
			{
				user->WriteNumeric(Numerics::NoSuchNick(targetnick));
				return CmdResult::FAILURE;
			}
		}

		if (!target->IsLocal())
			return CmdResult::SUCCESS; // Their server will handle this.

		const auto& newuser = parameters.back();
		if (newuser.size() > ServerInstance->Config->Limits.MaxUser)
		{
			IRCv3::WriteReply(Reply::FAIL, user, cap, this, "INVALID_USERNAME", "Username is too long");
			return CmdResult::FAILURE;
		}

		if (!ServerInstance->Users.IsUser(parameters[0]))
		{
			IRCv3::WriteReply(Reply::FAIL, user, cap, this, "INVALID_USERNAME", "Invalid characters in username");
			return CmdResult::FAILURE;
		}

		target->ChangeDisplayedUser(newuser);
		if (!user->server->IsService())
		{
			ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to change the username of {} to \"{}\x0F\"",
				user->GetRealMask(), user->GetAddress(), this->service_name, target->nick, newuser);
		}
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return parameters.size() > 1 ? ROUTE_OPT_UCAST(parameters[0]) : ROUTE_LOCALONLY;
	}
};

class ModuleSetIdent final
	: public Module
{
private:
	CommandSetIdent cmd;

public:
	ModuleSetIdent()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /SETIDENT command which allows server operators to change the username of users.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleSetIdent)
