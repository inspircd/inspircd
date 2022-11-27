/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
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
CmdResult CommandOpertype::HandleRemote(RemoteUser* u, CommandBase::Params& params)
{
	auto type = ServerInstance->Config->OperTypes.find(params[0]);
	if (type != ServerInstance->Config->OperTypes.end())
		u->OperLogin(std::make_shared<OperAccount>(type->first, type->second, ServerInstance->Config->EmptyTag));
	else
		u->OperLogin(std::make_shared<OperAccount>(params[0], nullptr, ServerInstance->Config->EmptyTag));

	if (Utils->quiet_bursts)
	{
		/*
		 * If quiet bursts are enabled, and server is bursting or a silent services server
		 * then do nothing. -- w00t
		 */
		TreeServer* remoteserver = TreeServer::Get(u);
		if (remoteserver->IsBehindBursting() || remoteserver->IsSilentService())
			return CmdResult::SUCCESS;
	}

	ServerInstance->SNO.WriteToSnoMask('O', "From %s: %s (%s) is now a server operator of type %s",
		u->server->GetName().c_str(), u->nick.c_str(), u->MakeHost().c_str(), u->oper->GetType().c_str());
	return CmdResult::SUCCESS;
}

CommandOpertype::Builder::Builder(User* user, const std::shared_ptr<OperAccount>& oper)
	: CmdBuilder(user, "OPERTYPE")
{
	push_last(oper->GetType());
}
