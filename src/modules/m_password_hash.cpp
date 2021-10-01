/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013, 2017-2018, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2010 Craig Edwards <brain@inspircd.org>
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
#include "modules/hash.h"

class CommandMkpasswd : public Command
{
 public:
	CommandMkpasswd(Module* Creator) : Command(Creator, "MKPASSWD", 2)
	{
		syntax = { "<hashtype> <plaintext>" };
		Penalty = 5;
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		if (!parameters[0].compare(0, 5, "hmac-", 5))
		{
			std::string type(parameters[0], 5);
			HashProvider* hp = ServerInstance->Modules.FindDataService<HashProvider>("hash/" + type);
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

			std::string salt = ServerInstance->GenRandomStr(hp->out_size, false);
			std::string target = hp->hmac(salt, parameters[1]);
			std::string str = Base64::Encode(salt) + "$" + Base64::Encode(target, NULL, 0);

			user->WriteNotice(parameters[0] + " hashed password for " + parameters[1] + " is " + str);
			return CmdResult::SUCCESS;
		}

		HashProvider* hp = ServerInstance->Modules.FindDataService<HashProvider>("hash/" + parameters[0]);
		if (!hp)
		{
			user->WriteNotice("Unknown hash type");
			return CmdResult::FAILURE;
		}

		try
		{
			std::string hexsum = hp->Generate(parameters[1]);
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
		auto tag = ServerInstance->Config->ConfValue("mkpasswd");
		cmd.access_needed = tag->getBool("operonly") ? CmdAccess::OPERATOR : CmdAccess::NORMAL;
	}

	ModResult OnPassCompare(Extensible* ex, const std::string& data, const std::string& input, const std::string& hashtype) override
	{
		if (!hashtype.compare(0, 5, "hmac-", 5))
		{
			std::string type(hashtype, 5);
			HashProvider* hp = ServerInstance->Modules.FindDataService<HashProvider>("hash/" + type);
			if (!hp)
				return MOD_RES_PASSTHRU;

			if (hp->IsKDF())
			{
				ServerInstance->Logs.Log(MODNAME, LOG_DEFAULT, "Tried to use HMAC with %s, which does not support HMAC", type.c_str());
				return MOD_RES_DENY;
			}

			// this is a valid hash, from here on we either accept or deny
			std::string::size_type sep = data.find('$');
			if (sep == std::string::npos)
				return MOD_RES_DENY;
			std::string salt = Base64::Decode(data.substr(0, sep));
			std::string target = Base64::Decode(data.substr(sep + 1));

			if (target == hp->hmac(salt, input))
				return MOD_RES_ALLOW;
			else
				return MOD_RES_DENY;
		}

		HashProvider* hp = ServerInstance->Modules.FindDataService<HashProvider>("hash/" + hashtype);

		/* Is this a valid hash name? */
		if (hp)
		{
			if (hp->Compare(input, data))
				return MOD_RES_ALLOW;
			else
				/* No match, and must be hashed, forbid */
				return MOD_RES_DENY;
		}

		// We don't handle this type, let other mods or the core decide
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModulePasswordHash)
