/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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
	// From RFC 2812.
	RPL_SERVLIST = 234,
	RPL_SERVLISTEND = 235
};

CommandServList::CommandServList(Module* parent)
	: SplitCommand(parent, "SERVLIST")
	, invisiblemode(parent, "invisible")
{
	allow_empty_last_param = false;
	syntax = { "[<mask>]" };
}

CmdResult CommandServList::HandleLocal(LocalUser* user, const Params& parameters)
{
	const std::string& mask = parameters.empty() ? "*" : parameters[0];
	for (auto* serviceuser : ServerInstance->Users.all_services)
	{
		if (serviceuser->IsModeSet(invisiblemode) || !InspIRCd::Match(serviceuser->nick, mask))
			continue;

		Numeric::Numeric numeric(RPL_SERVLIST);
		numeric
			.push(serviceuser->nick)
			.push(serviceuser->server->GetName())
			.push(mask)
			.push(0)
			.push(0)
			.push(serviceuser->GetRealName());
		user->WriteNumeric(numeric);
	}
	user->WriteNumeric(RPL_SERVLISTEND, mask, 0, "End of service listing");
	return CmdResult::SUCCESS;
}
