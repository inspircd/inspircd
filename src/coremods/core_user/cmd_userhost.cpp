/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2019, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
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
#include "core_user.h"

enum
{
	// From RFC 1459.
	RPL_USERHOST = 302,
};

CmdResult CommandUserhost::Handle(User* user, const Params& parameters)
{
	const bool has_privs = user->HasPrivPermission("users/auspex");

	std::string retbuf;

	size_t paramcount = std::min<size_t>(parameters.size(), 5);
	for (size_t i = 0; i < paramcount; ++i)
	{
		auto* u = ServerInstance->Users.FindNick(parameters[i], true);
		if (u)
		{
			retbuf += u->nick;

			if (u->IsOper())
			{
				// XXX: +H hidden opers must not be shown as opers
				if ((u == user) || (has_privs) || (!u->IsModeSet(hideopermode)))
					retbuf += '*';
			}

			retbuf += '=';
			retbuf += (u->IsAway() ? '-' : '+');
			retbuf += u->GetUser(u == user || has_privs);
			retbuf += '@';
			retbuf += u->GetHost(u == user || has_privs);
			retbuf += ' ';
		}
	}

	user->WriteNumeric(RPL_USERHOST, retbuf);

	return CmdResult::SUCCESS;
}
