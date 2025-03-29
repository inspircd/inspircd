/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020, 2022-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "timeutils.h"

#include "core_info.h"

CommandTime::CommandTime(Module* parent)
	: ServerTargetCommand(parent, "TIME")
{
	syntax = { "[<servername>]" };
}

CmdResult CommandTime::Handle(User* user, const Params& parameters)
{
	if (!parameters.empty() && !irc::equals(parameters[0], ServerInstance->Config->ServerName))
		return CmdResult::SUCCESS;

	auto timestr = Time::ToString(ServerInstance->Time(), Time::DEFAULT_LONG);
	timestr += FMT::format(" ({})", ServerInstance->Time());

	user->WriteRemoteNumeric(RPL_TIME, ServerInstance->Config->GetServerName(), timestr);
	return CmdResult::SUCCESS;
}
