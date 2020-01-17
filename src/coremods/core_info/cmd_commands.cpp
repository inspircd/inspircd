/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
	: Command(parent, "COMMANDS", 0, 0)
{
	Penalty = 3;
}

/** Handle /COMMANDS
 */
CmdResult CommandCommands::Handle(User* user, const Params& parameters)
{
	const CommandParser::CommandMap& commands = ServerInstance->Parser.GetCommands();
	std::vector<std::string> list;
	list.reserve(commands.size());
	for (CommandParser::CommandMap::const_iterator i = commands.begin(); i != commands.end(); ++i)
	{
		// Don't show S2S commands to users
		if (i->second->flags_needed == FLAG_SERVERONLY)
			continue;

		Module* src = i->second->creator;
		list.push_back(InspIRCd::Format("%s %s %d %d", i->second->name.c_str(), src->ModuleSourceFile.c_str(),
			i->second->min_params, i->second->Penalty));
	}
	std::sort(list.begin(), list.end());
	for(unsigned int i=0; i < list.size(); i++)
		user->WriteNumeric(RPL_COMMANDS, list[i]);
	user->WriteNumeric(RPL_COMMANDSEND, "End of COMMANDS list");
	return CMD_SUCCESS;
}
