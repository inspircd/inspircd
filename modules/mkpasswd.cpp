/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2025 Sadie Powell <sadie@witchery.services>
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
#include "modules/ircv3_replies.h"

class CommandMakePassword final
	: public SplitCommand
{
private:
	IRCv3::Replies::Fail failrpl;
	IRCv3::Replies::Note noterpl;
	IRCv3::Replies::CapReference stdrplcap;

public:
	CommandMakePassword(Module* mod)
		: SplitCommand(mod, "MKPASSWD", 2, 2)
		, failrpl(mod)
		, noterpl(mod)
		, stdrplcap(mod)
	{
		penalty = 5000;
		syntax = { "<hashtype> <plaintext>" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		auto* hp = ServerInstance->Modules.FindDataService<Hash::Provider>("hash/" + parameters[0]);
		if (!hp)
		{
			failrpl.SendIfCap(user, stdrplcap, this, "INVALID_HASH", parameters[0], FMT::format("{} is not a known hash algorithm!",
				parameters[0]));
			return CmdResult::FAILURE;
		}

		if (!hp->IsPasswordSafe())
		{
			failrpl.SendIfCap(user, stdrplcap, this, "INSECURE_HASH", hp->GetAlgorithm(), FMT::format("{} is not a secure password hashing algorithm!",
				hp->GetAlgorithm()));
			return CmdResult::FAILURE;
		}

		auto hash = hp->Hash(parameters[1]);
		if (hash.empty())
		{
			failrpl.SendIfCap(user, stdrplcap, this, "HASH_ERROR", hp->GetAlgorithm(), FMT::format("An error occurred whilst hashing your password with {}!",
				hp->GetAlgorithm()));
			return CmdResult::FAILURE;
		}

		auto phash = hp->ToPrintable(hash);
		noterpl.SendIfCap(user, stdrplcap, this, "HASH_RESULT", hp->GetAlgorithm(), phash, FMT::format("{} hashed password: {}",
			hp->GetAlgorithm(), phash));

		return CmdResult::SUCCESS;
	}
};

class ModuleMakePassword final
	: public Module
{
private:
	CommandMakePassword cmd;

public:
	ModuleMakePassword()
		: Module(VF_VENDOR, "Provides the /MKPASSWD command which allows the generation of hashed passwords for use in the server configuration.")
		, cmd(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("mkpasswd");
		cmd.access_needed = tag->getBool("operonly") ? CmdAccess::OPERATOR : CmdAccess::NORMAL;
	}
};

MODULE_INIT(ModuleMakePassword)
