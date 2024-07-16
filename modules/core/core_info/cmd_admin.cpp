/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
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

enum
{
	// From RFC 1459.
	RPL_ADMINME = 256,
	RPL_ADMINLOC1 = 257,
	RPL_ADMINLOC2  = 258,
	RPL_ADMINEMAIL = 259,
};

CommandAdmin::CommandAdmin(Module* parent)
	: ServerTargetCommand(parent, "ADMIN")
{
	penalty = 2000;
	syntax = { "[<servername>]" };
}

CmdResult CommandAdmin::Handle(User* user, const Params& parameters)
{
	if (!parameters.empty() && !irc::equals(parameters[0], ServerInstance->Config->ServerName))
		return CmdResult::SUCCESS;

	user->WriteRemoteNumeric(RPL_ADMINME, ServerInstance->Config->GetServerName(), "Administrative info");
	user->WriteRemoteNumeric(RPL_ADMINLOC1, adminname);
	if (!admindesc.empty())
	   user->WriteRemoteNumeric(RPL_ADMINLOC2, admindesc);
	user->WriteRemoteNumeric(RPL_ADMINEMAIL, adminemail);
	return CmdResult::SUCCESS;
}
