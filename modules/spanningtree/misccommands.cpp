/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
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

#include "main.h"
#include "commands.h"
#include "treeserver.h"

CmdResult CommandSNONotice::Handle(User* user, Params& params)
{
	ServerInstance->SNO.WriteToSnoMask(params[0][0], "From " + user->nick + ": " + params[1]);
	return CmdResult::SUCCESS;
}

CmdResult CommandEndBurst::HandleServer(TreeServer* server, Params& params)
{
	server->FinishBurst();
	return CmdResult::SUCCESS;
}
