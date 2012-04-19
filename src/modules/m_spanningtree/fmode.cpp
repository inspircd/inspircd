/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "commands.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/** FMODE command - server mode with timestamp checks */
CmdResult CommandFMode::Handle(const std::vector<std::string>& params, User *who)
{
	std::string sourceserv = who->server;
	if (IS_SERVER(who))
	{
		SpanningTreeUtilities* Utils = ((ModuleSpanningTree*)(Module*)creator)->Utils;
		TreeServer* origin = Utils->FindServer(sourceserv);
		if (origin->GetSocket()->proto_version < 1203 && params[2][0] == '+')
			const_cast<parameterlist&>(params)[2][0] = '=';
	}

	std::vector<std::string> mode_list;
	mode_list.push_back(params[0]);
	time_t TS = atoi(params[1].c_str());
	if (!TS)
		return CMD_INVALID;
	mode_list.insert(mode_list.end(), params.begin() + 2, params.end());

	Extensible* target;
	irc::modestacker modes;
	ServerInstance->Modes->Parse(mode_list, who, target, modes);

	// maybe last user already parted the channel; discard if so.
	if (!target)
		return CMD_FAILURE;
	
	time_t ourTS = 0;
	if (params[0][0] == '#') {
		ourTS = static_cast<Channel*>(target)->age;
	} else {
		ourTS = static_cast<User*>(target)->age;
	}

	// given TS is greater: the change fails to propagate (possible desync takeover)
	if (TS > ourTS)
		return CMD_FAILURE;

	// netburst merge: only if equal, and new in 2.1, only if using =
	bool merge = (TS == ourTS && mode_list[1][0] == '=');

	ServerInstance->Modes->Process(who, target, modes, merge);
	ServerInstance->Modes->Send(who, target, modes);
	return CMD_SUCCESS;
}
