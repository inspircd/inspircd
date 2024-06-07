/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017, 2019-2023 Sadie Powell <sadie@witchery.services>
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

class CommandSethost final
	: public Command
{
public:
	CharState hostmap;

	CommandSethost(Module* Creator)
		: Command(Creator, "SETHOST", 1)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<host>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (parameters[0].length() > ServerInstance->Config->Limits.MaxHost)
		{
			user->WriteNotice("*** SETHOST: Host too long");
			return CmdResult::FAILURE;
		}

		for (const auto chr : parameters[0])
		{
			if (!hostmap.test(static_cast<unsigned char>(chr)))
			{
				user->WriteNotice("*** SETHOST: Invalid characters in hostname");
				return CmdResult::FAILURE;
			}
		}

		user->ChangeDisplayedHost(parameters[0]);
		ServerInstance->SNO.WriteGlobalSno('a', user->nick+" used SETHOST to change their displayed host to "+user->GetDisplayedHost());
		return CmdResult::SUCCESS;
	}
};

class ModuleSetHost final
	: public Module
{
private:
	CommandSethost cmd;

public:
	ModuleSetHost()
		: Module(VF_VENDOR, "Adds the /SETHOST command which allows server operators to change their displayed hostname.")
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
				throw ModuleException(this, INSP_FORMAT("<hostname:charmap> can not contain character 0x{:02X} ({})", chr, chr));
			newhostmap.set(static_cast<unsigned char>(chr));
		}
		std::swap(newhostmap, cmd.hostmap);
	}

	void GetLinkData(Module::LinkData& data, std::string& compatdata) override
	{
		for (size_t i = 0; i < cmd.hostmap.size(); ++i)
			if (cmd.hostmap[i])
				data["hostchars"].push_back(static_cast<unsigned char>(i));
	}
};

MODULE_INIT(ModuleSetHost)
