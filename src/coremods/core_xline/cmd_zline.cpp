/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018, 2020, 2022-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Matt Smith <dz@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007-2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "xline.h"
#include "core_xline.h"

CommandZline::CommandZline(Module* parent)
	: Command(parent, "ZLINE", 1, 3)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<ipmask>[,<ipmask>]+ [[<duration>] :<reason>]" };
}

CmdResult CommandZline::Handle(User* user, const Params& parameters)
{
	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CmdResult::SUCCESS;

	std::string target = parameters[0];
	if (parameters.size() > 1)
	{
		if (target.find('!') != std::string::npos)
		{
			user->WriteNotice("*** You cannot include a nickname in a Z-line, a Z-line must ban only an IP mask.");
			return CmdResult::FAILURE;
		}

		auto* u = ServerInstance->Users.Find(target, true);
		if (u)
		{
			target = u->GetAddress();
		}

		const char* ipaddr = target.c_str();

		if (strchr(ipaddr, '@'))
		{
			while (*ipaddr != '@')
				ipaddr++;
			ipaddr++;
		}

		IPMatcher matcher;
		if (InsaneBan::MatchesEveryone(ipaddr, matcher, user, 'Z', "ipmasks"))
			return CmdResult::FAILURE;

		unsigned long duration = 0;
		if (parameters.size() > 2 && !Duration::TryFrom(parameters[1], duration))
		{
			user->WriteNotice("*** Invalid duration for Z-line.");
			return CmdResult::FAILURE;
		}

		auto* zl = new ZLine(ServerInstance->Time(), duration, user->nick, parameters.back(), ipaddr);
		if (ServerInstance->XLines->AddLine(zl, user))
		{
			if (!duration)
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a permanent Z-line on {}: {}",
					user->nick, ipaddr, zl->reason);
			}
			else
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a timed Z-line on {}, expires in {} (on {}): {}",
					user->nick, ipaddr, Duration::ToString(duration), Time::FromNow(duration), zl->reason);
			}
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete zl;
			user->WriteNotice("*** Z-line for " + std::string(ipaddr) + " already exists.");
		}
	}
	else
	{
		std::string reason;

		if (ServerInstance->XLines->DelLine(target, "Z", reason, user))
		{
			ServerInstance->SNO.WriteToSnoMask('x', "{} removed Z-line on {}: {}", user->nick, target, reason);
		}
		else
		{
			user->WriteNotice("*** Z-line " + target + " not found on the list.");
			return CmdResult::FAILURE;
		}
	}

	return CmdResult::SUCCESS;
}

bool CommandZline::IPMatcher::Check(User* user, const std::string& ip) const
{
	return InspIRCd::MatchCIDR(user->GetAddress(), ip, ascii_case_insensitive_map);
}
