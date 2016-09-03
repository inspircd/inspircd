/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

#include "utils.h"
#include "treeserver.h"
#include "commands.h"
#include "utils.h"

CmdResult CommandPong::HandleServer(TreeServer* server, std::vector<std::string>& params)
{
	if (server->IsBursting())
	{
		ServerInstance->SNO->WriteGlobalSno('l', "Server \002%s\002 has not finished burst, forcing end of burst (send ENDBURST!)", server->GetName().c_str());
		server->FinishBurst();
	}

	if (params[0] == ServerInstance->Config->GetSID())
	{
		// PONG for us
		server->OnPong();
	}
	return CMD_SUCCESS;
}
