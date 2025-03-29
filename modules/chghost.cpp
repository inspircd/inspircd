/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013-2014, 2017, 2019-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009, 2012 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "numerichelper.h"

class CommandChghost final
	: public Command
{
public:
	CharState hostmap;

	CommandChghost(Module* Creator)
		: Command(Creator, "CHGHOST", 2)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<nick> <host>" };
		translation = { TR_NICK, TR_TEXT };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (parameters[1].length() > ServerInstance->Config->Limits.MaxHost)
		{
			user->WriteNotice("*** CHGHOST: Host too long");
			return CmdResult::FAILURE;
		}

		for (const auto chr : parameters[1])
		{
			if (!hostmap.test(static_cast<unsigned char>(chr)))
			{
				user->WriteNotice("*** CHGHOST: Invalid characters in hostname");
				return CmdResult::FAILURE;
			}
		}

		auto* dest = ServerInstance->Users.Find(parameters[0]);

		// Allow services to change the host of partially connected users.
		if (!dest || (!dest->IsFullyConnected() && !user->server->IsService()))
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CmdResult::FAILURE;
		}

		if (IS_LOCAL(dest))
		{
			dest->ChangeDisplayedHost(parameters[1]);
			if (!user->server->IsService())
			{
				ServerInstance->SNO.WriteGlobalSno('a', user->nick+" used CHGHOST to make the displayed host of "+dest->nick+" become "+dest->GetDisplayedHost());
			}
		}

		return CmdResult::SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const Params& parameters) override
	{
		return ROUTE_OPT_UCAST(parameters[0]);
	}
};

class ModuleChgHost final
	: public Module
{
private:
	CommandChghost cmd;

public:
	ModuleChgHost()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds the /CHGHOST command which allows server operators to change the displayed hostname of a user.")
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("hostname");
		const std::string hmap = tag->getString("charmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.-_/0123456789", 1);

		CharState newhostmap;
		for (const auto chr : hmap)
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

MODULE_INIT(ModuleChgHost)
