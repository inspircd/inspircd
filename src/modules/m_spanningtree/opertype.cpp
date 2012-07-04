/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "treeserver.h"
#include "utils.h"

/** Because the core won't let users or even SERVERS set +o,
 * we use the OPERTYPE command to do this.
 */
CmdResult CommandOpertype::Handle(const std::vector<std::string>& params, User *u)
{
	SpanningTreeUtilities* Utils = ((ModuleSpanningTree*)(Module*)creator)->Utils;
	std::string opertype = params[0];
	if (!IS_OPER(u))
		ServerInstance->Users->all_opers.push_back(u);
	u->modes[UM_OPERATOR] = 1;
	OperIndex::iterator iter = ServerInstance->Config->oper_blocks.find(" " + opertype);
	if (iter != ServerInstance->Config->oper_blocks.end())
		u->oper = iter->second;
	else
	{
		u->oper = new OperInfo;
		u->oper->name = opertype;
	}

	if (Utils->quiet_bursts)
	{
		/*
		 * If quiet bursts are enabled, and server is bursting or silent uline (i.e. services),
		 * then do nothing. -- w00t
		 */
		TreeServer* remoteserver = Utils->FindServer(u->server);
		if (remoteserver->bursting || ServerInstance->SilentULine(u->server))
			return CMD_SUCCESS;
	}

	ServerInstance->SNO->WriteToSnoMask('O',"From %s: User %s (%s@%s) is now an IRC operator of type %s",u->server.c_str(), u->nick.c_str(),u->ident.c_str(), u->host.c_str(), irc::Spacify(opertype.c_str()));
	return CMD_SUCCESS;
}

