/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017, 2019-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

class CommandSetHost final
	: public Command
{
public:
	IRCv3::ReplyCapReference cap;
	CharState hostmap;

	CommandSetHost(Module* mod)
		: Command(mod, "SETHOST", 1, 2)
		, cap(mod)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "[<nick>] :<newhost>" };
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
				if (user != target && !user->HasPrivPermission("users/sethost-others"))
				{
					user->WriteNumeric(Numerics::NoPrivileges("your server operator account does not have the users/sethost-others privilege"));
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

		const auto& newhost = parameters.back();
		if (newhost.size() > ServerInstance->Config->Limits.MaxHost)
		{
			IRCv3::WriteReply(Reply::FAIL, user, cap, this, "INVALID_HOSTNAME", "Hostname is too long");
			return CmdResult::FAILURE;
		}

		for (const auto chr : newhost)
		{
			if (!hostmap.test(static_cast<unsigned char>(chr)))
			{
				IRCv3::WriteReply(Reply::FAIL, user, cap, this, "INVALID_HOSTNAME", "Invalid characters in hostname");
				return CmdResult::FAILURE;
			}
		}

		target->ChangeDisplayedHost(newhost);
		if (!user->server->IsService())
		{
			ServerInstance->SNO.WriteGlobalSno('a', "{} ({}) used {} to change the hostname of {} to \"{}\x0F\"",
				user->GetRealMask(), user->GetAddress(), this->service_name, target->nick, newhost);
		}
		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return parameters.size() > 1 ? ROUTE_OPT_UCAST(parameters[0]) : ROUTE_LOCALONLY;
	}
};

class ModuleSetHost final
	: public Module
{
private:
	CommandSetHost cmd;

public:
	ModuleSetHost()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /SETHOST command which allows server operators to change the hostname of users.")
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("hostname");

		CharState newhostmap;
		for (const auto chr : tag->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789", 1))
		{
			// A hostname can not contain NUL, LF, CR, or SPACE.
			if (chr == 0x00 || chr == 0x0A || chr == 0x0D || chr == 0x20)
				throw ModuleException(this, "<hostname:charmap> can not contain character 0x{:02X} ({})", chr, chr);
			newhostmap.set(static_cast<unsigned char>(chr));
		}
		std::swap(newhostmap, cmd.hostmap);
	}

	void GetLinkData(Module::LinkData& data) override
	{
		for (size_t i = 0; i < cmd.hostmap.size(); ++i)
			if (cmd.hostmap[i])
				data["hostchars"].push_back(static_cast<unsigned char>(i));
	}
};

MODULE_INIT(ModuleSetHost)
