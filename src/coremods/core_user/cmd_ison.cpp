/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "numericbuilder.h"

#include "core_user.h"

enum
{
	// From RFC 1459.
	RPL_ISON = 303,
};

class IsonReplyBuilder final
	: public Numeric::Builder<' ', true>
{
public:
	IsonReplyBuilder(LocalUser* user)
		: Numeric::Builder<' ', true>(user, RPL_ISON)
	{
	}

	void AddNick(const std::string& nickname)
	{
		User* const user = ServerInstance->Users.FindNick(nickname, true);
		if (user)
			Add(user->nick);
	}
};

CmdResult CommandIson::HandleLocal(LocalUser* user, const Params& parameters)
{
	IsonReplyBuilder reply(user);

	for (std::vector<std::string>::const_iterator i = parameters.begin(); i != parameters.end()-1; ++i)
	{
		const std::string& targetstr = *i;
		reply.AddNick(targetstr);
	}

	// Last parameter can be a space separated list
	irc::spacesepstream ss(parameters.back());
	for (std::string token; ss.GetToken(token); )
		reply.AddNick(token);

	reply.Flush();
	return CmdResult::SUCCESS;
}
