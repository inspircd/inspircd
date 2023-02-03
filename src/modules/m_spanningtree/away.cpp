/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "utils.h"
#include "commands.h"

CmdResult CommandAway::HandleRemote(::RemoteUser* u, Params& params)
{
	if (!params.empty())
	{
		if (params.size() > 1)
			u->awaytime = ServerCommand::ExtractTS(params[0]);
		else
			u->awaytime = ServerInstance->Time();

		u->awaymsg = params.back();
		awayevprov.Call(&Away::EventListener::OnUserAway, u);
	}
	else
	{
		const std::string awaymsg = u->awaymsg;
		u->awaytime = 0;
		u->awaymsg.clear();
		awayevprov.Call(&Away::EventListener::OnUserBack, u, awaymsg);
	}
	return CmdResult::SUCCESS;
}

CommandAway::Builder::Builder(User* user)
	: CmdBuilder(user, "AWAY")
{
	if (!user->awaymsg.empty())
		push_int(user->awaytime).push_last(user->awaymsg);
}
