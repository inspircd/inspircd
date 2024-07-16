/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
#include "modules/whois.h"
#include "numerichelper.h"

#include "main.h"
#include "commandbuilder.h"

ModResult ModuleSpanningTree::HandleRemoteWhois(const CommandBase::Params& parameters, User* user)
{
	auto* remote = ServerInstance->Users.FindNick(parameters[1]);
	if (remote && !IS_LOCAL(remote))
	{
		CmdBuilder(user, "IDLE").push(remote->uuid).Unicast(remote);
		return MOD_RES_DENY;
	}
	else if (!remote)
	{
		user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
		user->WriteNumeric(RPL_ENDOFWHOIS, parameters[0], "End of /WHOIS list.");
		return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}
