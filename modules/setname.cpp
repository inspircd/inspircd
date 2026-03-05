/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2022 delthas
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
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
#include "modules/monitor.h"
#include "numerichelper.h"

class CommandSetName final
	: public Command
{
public:
	Cap::Capability cap;
	bool notifyopers;

	CommandSetName(Module* mod)
		: Command(mod, "SETNAME", 1, 2)
		, cap(mod, "setname")
	{
		syntax = { "[<nick>] :<newreal>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		auto* target = user;
		if (parameters.size() > 1)
		{
			const auto& targetnick = parameters[0];
			if (IS_LOCAL(user))
			{
				// For local users we need to check if they can use the command.
				target = ServerInstance->Users.FindNick(targetnick, true);
				if (user != target && !user->HasPrivPermission("users/setname-others"))
				{
					user->WriteNumeric(Numerics::NoPrivileges(user, "your server operator account does not have the users/setname-others privilege"));
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

		if (!IS_LOCAL(target))
			return CmdResult::SUCCESS; // Their server will handle this.

		const auto& newreal = parameters.back();
		if (newreal.size() > ServerInstance->Config->Limits.MaxReal)
		{
			IRCv3::WriteReply(Reply::FAIL, user, &cap, this, "INVALID_REALNAME", "Real name is too long");
			return CmdResult::FAILURE;
		}

		target->ChangeRealName(newreal);
		if (notifyopers || user != target)
		{
			ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to change the real name of {} to \"{}\x0F\"",
				user->GetRealMask(), user->GetAddress(), this->service_name, target->nick, newreal);
		}
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return parameters.size() > 1 ? ROUTE_OPT_UCAST(parameters[0]) : ROUTE_LOCALONLY;
	}
};

class ModuleSetName final
	: public Module
{
private:
	CommandSetName cmd;
	ClientProtocol::EventProvider setnameevprov;
	Monitor::API monitorapi;

public:
	ModuleSetName()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /SETNAME command which allows users to change their real name.")
		, cmd(this)
		, setnameevprov(this, "SETNAME")
		, monitorapi(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("setname");

		// Whether the module should only be usable by server operators.
		bool operonly = tag->getBool("operonly");
		cmd.access_needed = operonly ? CmdAccess::OPERATOR : CmdAccess::NORMAL;

		// Whether a snotice should be sent out when a user changes their real name.
		cmd.notifyopers = tag->getBool("notifyopers", !operonly);
	}

	void OnChangeRealName(User* user, const std::string& real) override
	{
		if (!(user->connected & User::CONN_NICKUSER))
			return;

		ClientProtocol::Message msg("SETNAME", user);
		msg.PushParamRef(real);
		ClientProtocol::Event protoev(setnameevprov, msg);
		IRCv3::WriteNeighborsWithCap res(user, protoev, cmd.cap, true);
		Monitor::WriteWatchersWithCap(monitorapi, user, protoev, cmd.cap, res.GetAlreadySentId());
	}
};

MODULE_INIT(ModuleSetName)
