/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013, 2017, 2019-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "modules/newhash.h"

class CommandMkpasswd final
	: public Command
{
public:
	CommandMkpasswd(Module* Creator)
		: Command(Creator, "MKPASSWD", 2)
	{
		penalty = 5000;
		syntax = { "<hashtype> <plaintext>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (!parameters[0].compare(0, 5, "hmac-", 5))
		{
			std::string type(parameters[0], 5);
			auto* hp = ServerInstance->Modules.FindDataService<Hash::Provider>("hash/" + type);
			if (!hp)
			{
				user->WriteNotice("Unknown hash type");
				return CmdResult::FAILURE;
			}

			if (hp->IsKDF())
			{
				user->WriteNotice(type + " does not support HMAC");
				return CmdResult::FAILURE;
			}

			std::string salt(hp->digest_size, '\0');
			ServerInstance->GenRandom(salt.data(), salt.length());

			std::string target = Hash::HMAC(hp, salt, parameters[1]);
			std::string str = Base64::Encode(salt) + "$" + Base64::Encode(target, nullptr, 0);

			user->WriteNotice(parameters[0] + " hashed password for " + parameters[1] + " is " + str);
			return CmdResult::SUCCESS;
		}

		auto* hp = ServerInstance->Modules.FindDataService<Hash::Provider>("hash/" + parameters[0]);
		if (!hp)
		{
			user->WriteNotice("Unknown hash type");
			return CmdResult::FAILURE;
		}

		try
		{
			auto hexsum = hp->ToPrintable(hp->Hash(parameters[1]));
			user->WriteNotice(parameters[0] + " hashed password for " + parameters[1] + " is " + hexsum);
			return CmdResult::SUCCESS;
		}
		catch (const ModuleException& error)
		{
			user->WriteNotice("*** " + name + ": " + error.GetReason());
			return CmdResult::FAILURE;
		}
	}
};

class ModulePasswordHash final
	: public Module
{
private:
	CommandMkpasswd cmd;

public:
	ModulePasswordHash()
		: Module(VF_VENDOR, "Allows passwords to be hashed and adds the /MKPASSWD command which allows the generation of hashed passwords for use in the server configuration.")
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("mkpasswd");
		cmd.access_needed = tag->getBool("operonly") ? CmdAccess::OPERATOR : CmdAccess::NORMAL;
	}
};

MODULE_INIT(ModulePasswordHash)
