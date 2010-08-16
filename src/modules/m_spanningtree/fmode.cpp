/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
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
		if (origin->GetRoute()->GetSocket()->proto_version < 1203 && params[2][0] == '+')
			const_cast<parameterlist&>(params)[2][0] = '=';
	}

	std::vector<std::string> modelist;
	modelist.push_back(params[0]);
	time_t TS = atoi(params[1].c_str());
	if (!TS)
		return CMD_INVALID;
	modelist.insert(modelist.end(), params.begin() + 2, params.end());

	Extensible* target;
	irc::modestacker modes;
	ServerInstance->Modes->Parse(modelist, who, target, modes);

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
	bool merge = (TS == ourTS && modelist[1][0] == '=');

	ServerInstance->Modes->Process(who, target, modes, merge);
	ServerInstance->Modes->Send(who, target, modes);
	return CMD_SUCCESS;
}
