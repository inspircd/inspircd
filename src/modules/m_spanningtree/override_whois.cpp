/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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
#include "commandbuilder.h"

ModResult ModuleSpanningTree::HandleRemoteWhois(const CommandBase::Params& parameters, User* user)
{
	User* remote = ServerInstance->FindNickOnly(parameters[1]);
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
