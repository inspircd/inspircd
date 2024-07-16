/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020, 2022-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "core_info.h"

CommandVersion::CommandVersion(Module* parent, ISupportManager& isupportmgr)
	: Command(parent, "VERSION")
	, isupport(isupportmgr)
	, operversion(RPL_VERSION)
	, version(RPL_VERSION)
{
	syntax = { "[<servername>]" };
}

void CommandVersion::BuildNumerics()
{
	version.GetParams().clear();
	version.push(INSPIRCD_BRANCH ".");
	version.push(ServerInstance->Config->GetServerName());
	version.push(ServerInstance->Config->CustomVersion);

	operversion.GetParams().clear();
	operversion.push(INSPIRCD_VERSION ".");
	operversion.push(ServerInstance->Config->ServerName);
	operversion.push("[" + ServerInstance->Config->ServerId + "] " + ServerInstance->Config->CustomVersion);
}

CmdResult CommandVersion::Handle(User* user, const Params& parameters)
{
	user->WriteNumeric(user->IsOper() ? operversion : version);
	LocalUser* luser = IS_LOCAL(user);
	if (luser)
		isupport.SendTo(luser);
	return CmdResult::SUCCESS;
}
