/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2023 Sadie Powell <sadie@sadiepowell.dev>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014-2015, 2018 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
#include "clientprotocolmsg.h"

#include "core_oper.h"

CommandDie::CommandDie(const WeakModulePtr& parent)
	: Command(parent, "DIE", 1, 1)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<servername>" };
}

CmdResult CommandDie::Handle(User* user, const Params& parameters)
{
	if (insp::casemapped_equals(parameters[0], ServerInstance->Config->ServerName))
	{
		ServerInstance->SNO.WriteGlobalSno('a', "DIE command from {}, shutting down server.", user->GetRealMask());
		ServerInstance->Exit(EXIT_SUCCESS);
	}
	return CmdResult::FAILURE;
}
