/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023, 2025 Sadie Powell <sadie@sadiepowell.dev>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <brain@inspircd.org>
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
#include "timeutils.h"

class CommandSetIdle final
	: public Command
{
private:
	IRCv3::ReplyCapReference stdrplcap;

public:
	CommandSetIdle(const WeakModulePtr& Creator)
		: Command(Creator, "SETIDLE", 1, 2)
		, stdrplcap(Creator)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "[<nick>] <duration>" };
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
				if (user != target && !user->HasPrivPermission("users/setidle-others"))
				{
					user->WriteNumeric(Numerics::NoPrivileges("your server operator account does not have the users/setidle-others privilege"));
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

		auto* ltarget = target->AsLocal();
		if (!ltarget)
			return CmdResult::SUCCESS; // Their server will handle this.

		const auto& newidle = parameters.back();
		unsigned long idle;
		if (!Duration::TryFrom(newidle, idle))
		{
			IRCv3::WriteReply(Reply::FAIL, user, stdrplcap, this, "INVALID_IDLE_TIME", newidle, "Invalid idle time.");
			return CmdResult::FAILURE;
		}

		ltarget->idle_lastmsg = (ServerInstance->Time() - idle);
		// We cant have signon time shorter than our idle time!
		if (ltarget->signon > ltarget->idle_lastmsg)
			ltarget->signon = ltarget->idle_lastmsg;

		if (!user->server->IsService())
		{
			ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to change the idle time of {} to {}",
				user->GetRealMask(), user->GetAddress(), this->service_name, target->nick, Duration::ToLongString(idle));
		}
		IRCv3::WriteReply(Reply::NOTE, user, stdrplcap, this, "IDLE_TIME_SET", idle, "Idle time set.");
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return parameters.size() > 1 ? ROUTE_OPT_UCAST(parameters[0]) : ROUTE_LOCALONLY;
	}
};

class ModuleSetIdle final
	: public Module
{
private:
	CommandSetIdle cmd;

public:
	ModuleSetIdle()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /SETIDLE command which allows server operators to change the idle time of users.")
		, cmd(weak_from_this())
	{
	}
};

MODULE_INIT(ModuleSetIdle)
