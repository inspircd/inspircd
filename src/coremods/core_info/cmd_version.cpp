/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2010 Craig Edwards <brain@inspircd.org>
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
	: Command(parent, "VERSION", 0, 0)
	, isupport(isupportmgr)
{
	syntax = { "[<servername>]" };
}

CmdResult CommandVersion::Handle(User* user, const Params& parameters)
{
	Numeric::Numeric numeric(RPL_VERSION);
	if (user->IsOper())
	{
		numeric.push(INSPIRCD_VERSION);
		numeric.push(ServerInstance->Config->ServerName);
		numeric.push("[" + ServerInstance->Config->GetSID() + "] " + ServerInstance->Config->CustomVersion);
	}
	else
	{
		numeric.push(INSPIRCD_BRANCH);
		numeric.push(ServerInstance->Config->GetServerName());
		numeric.push(ServerInstance->Config->CustomVersion);
	}
	user->WriteNumeric(numeric);

	LocalUser* luser = IS_LOCAL(user);
	if (luser)
		isupport.SendTo(luser);
	return CmdResult::SUCCESS;
}
