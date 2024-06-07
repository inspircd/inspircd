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

enum
{
	// InspIRCd-specific.
	ERR_AMBIGUOUSCOMMAND = 420
};

class ModuleAbbreviation final
	: public Module
{
public:
	ModuleAbbreviation()
		: Module(VF_VENDOR, "Allows commands to be abbreviated by appending a full stop.")
	{
	}

	void Prioritize() override
	{
		ServerInstance->Modules.SetPriority(this, I_OnPreCommand, PRIORITY_FIRST);
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		/* Command is already validated, has a length of 0, or last character is not a . */
		if (validated || command.empty() || command.back() != '.')
			return MOD_RES_PASSTHRU;

		/* Look for any command that starts with the same characters, if it does, replace the command string with it */
		size_t clen = command.length() - 1;
		std::string foundcommand;
		std::string matchlist;
		bool foundmatch = false;
		for (const auto& [cmdname, _] : ServerInstance->Parser.GetCommands())
		{
			if (!command.compare(0, clen, cmdname, 0, clen))
			{
				if (matchlist.length() > 450)
				{
					user->WriteNumeric(ERR_AMBIGUOUSCOMMAND, "Ambiguous abbreviation and too many possible matches.");
					return MOD_RES_DENY;
				}

				if (!foundmatch)
				{
					/* Found the command */
					foundcommand = cmdname;
					foundmatch = true;
				}
				else
					matchlist.append(" ").append(cmdname);
			}
		}

		/* Ambiguous command, list the matches */
		if (!matchlist.empty())
		{
			user->WriteNumeric(ERR_AMBIGUOUSCOMMAND, fmt::format("Ambiguous abbreviation, possible matches: {}{}", foundcommand, matchlist));
			return MOD_RES_DENY;
		}

		if (!foundcommand.empty())
		{
			command = foundcommand;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleAbbreviation)
