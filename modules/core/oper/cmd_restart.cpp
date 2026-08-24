/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022-2023 Sadie Powell <sadie@sadiepowell.dev>
 *   Copyright (C) 2017 Jordyn <onlinecloud1@gmail.com>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "core_oper.h"

CommandRestart::CommandRestart(const WeakModulePtr& parent)
	: Command(parent, "RESTART", 1, 1)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<servername>" };
}

CmdResult CommandRestart::Handle(User* user, const Params& parameters)
{
	if (insp::casemapped_equals(parameters[0], ServerInstance->Config->ServerName))
	{
		ServerInstance->SNO.WriteGlobalSno('a', "RESTART command from {}, restarting server.", user->GetRealMask());
		const auto error = ServerInstance->Restart();
		ServerInstance->SNO.WriteGlobalSno('a', "Failed RESTART: {}", error);
	}
	return CmdResult::FAILURE;
}
