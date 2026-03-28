/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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

class ModuleAbbreviation final
	: public Module
{
private:
	IRCv3::ReplyCapReference stdrplcap;

public:
	ModuleAbbreviation()
		: Module(VF_VENDOR, "Allows commands to be abbreviated by appending a full stop.")
		, stdrplcap(weak_from_this())
	{
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(shared_from_this(), I_OnPreCommand, PRIORITY_FIRST);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		/* Command is already validated, has a length of 0, or last character is not a . */
		if (validated || command.empty() || command.back() != '.')
			return MOD_RES_PASSTHRU;

		// :irc.example.com FAIL * AMBIGUOUS_ABBREVIATION :BLAH
		// :irc.example.com NOTICE nick :*** BLAH
		const auto maxlen = ServerInstance->Config->Limits.MaxLine
			- ServerInstance->Config->GetServerName().size()
			- std::max<size_t>(user->nick.length() + 4, 22)
			- 10; // Some extra just in case.

		auto foundmatch = false;
		std::string foundcommand;
		std::string extracommands;

		const auto cmdlen = command.length() - 1;
		for (const auto& [cmdname, cmd] : ServerInstance->Parser.GetCommands())
		{
			// Look for any command that starts with the same characters that
			// is usable by the caller.
			if (command.compare(0, cmdlen, cmdname, 0, cmdlen) != 0 || !cmd->IsUsableBy(user))
				continue; // No match.

			if (extracommands.length() > maxlen)
			{
				IRCv3::WriteReply(Reply::FAIL, user, stdrplcap, nullptr, "AMBIGUOUS_ABBREVIATION",
					"Ambiguous abbreviation and too many possible matches.");
				return MOD_RES_DENY;
			}

			if (!foundmatch)
			{
				// Found the command.
				foundcommand = cmdname;
				foundmatch = true;
			}
			else
				extracommands.append(" ").append(cmdname);
		}

		/* Ambiguous command, list the matches */
		if (!extracommands.empty())
		{
			IRCv3::WriteReply(Reply::FAIL, user, stdrplcap, nullptr, "AMBIGUOUS_ABBREVIATION",
				FMT::format("Ambiguous abbreviation, possible matches: {}{}", foundcommand, extracommands));
			return MOD_RES_DENY;
		}

		// Replace the command abbreviation with the found command name.
		if (!foundcommand.empty())
			command = foundcommand;

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleAbbreviation)
