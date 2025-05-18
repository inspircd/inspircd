/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2020-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
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
#include "core_info.h"

enum
{
	// InspIRCd-specific.
	RPL_COMMANDS = 700,
	RPL_COMMANDSEND = 701
};

CommandCommands::CommandCommands(Module* parent)
	: SplitCommand(parent, "COMMANDS")
{
	penalty = 3000;
}

CmdResult CommandCommands::HandleLocal(LocalUser* user, const Params& parameters)
{
	std::vector<Numeric::Numeric> numerics;
	numerics.reserve(ServerInstance->Parser.GetCommands().size());

	const auto has_auspex = user->HasPrivPermission("servers/auspex");
	for (const auto& [_, command] : ServerInstance->Parser.GetCommands())
	{
		// Don't show privileged commands to users without the privilege.
		bool usable = true;
		switch (command->access_needed)
		{
			case CmdAccess::NORMAL: // Everyone can use user commands.
				break;

			case CmdAccess::OPERATOR: // Only opers can use oper commands.
				usable = user->HasCommandPermission(command->name);
				break;

			case CmdAccess::SERVER: // Nobody can use server commands.
				usable = false;
				break;
		}

		// Only send this command to the user if:
		// 1. It is usable by the caller.
		// 2. The caller has the servers/auspex priv.
		if (!usable && !has_auspex)
			continue;

		Numeric::Numeric numeric(RPL_COMMANDS);
		numeric.push(command->name);
		numeric.push(ModuleManager::ShrinkModName(command->creator->ModuleFile));
		numeric.push(command->min_params);
		if (command->max_params < command->min_params)
			numeric.push("*");
		else
			numeric.push(command->max_params);
		numeric.push(command->penalty);
		numerics.push_back(numeric);
	}

	// Sort alphabetically by command name.
	std::sort(numerics.begin(), numerics.end(), [](const auto& lhs, const auto& rhs) {
		return lhs.GetParams()[0] < rhs.GetParams()[0];
	});

	for (const auto& numeric : numerics)
		user->WriteNumeric(numeric);

	user->WriteNumeric(RPL_COMMANDSEND, "End of COMMANDS list");
	return CmdResult::SUCCESS;
}
