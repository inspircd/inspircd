/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018, 2020-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006-2008 Craig Edwards <brain@inspircd.org>
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

CommandEline::CommandEline(Module* parent)
	: Command(parent, "ELINE", 1, 4)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<user@host>[,<user@host>]+ [[<duration>] [LOCAL] :<reason>]" };
}

CmdResult CommandEline::Handle(User* user, const Params& parameters)
{
	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CmdResult::SUCCESS;

	std::string target = parameters[0];
	if (parameters.size() > 1)
	{
		UserHostPair ih;
		auto* find = ServerInstance->Users.Find(target, true);
		if (find)
		{
			ih.first = find->GetBanUser(true);
			ih.second = find->GetAddress();
			target = ih.first + "@" + ih.second;
		}
		else
			ih = XLineManager::SplitUserHost(target);

		if (ih.first.empty())
		{
			user->WriteNotice("*** Target not found.");
			return CmdResult::FAILURE;
		}

		InsaneBan::IPHostMatcher matcher;
		if (InsaneBan::MatchesEveryone(ih.first + "@" + ih.second, matcher, user, 'E', "hostmasks"))
			return CmdResult::FAILURE;

		unsigned long duration = 0;
		if (parameters.size() > 2 && !Duration::TryFrom(parameters[1], duration))
		{
			user->WriteNotice("*** Invalid duration for E-line.");
			return CmdResult::FAILURE;
		}

		auto local = false;
		auto reason = parameters.back();
		if (parameters.size() > 3 && irc::equals(parameters[2], "LOCAL"))
			local = true;
		else
			reason.insert(0, " ").insert(0, parameters[2]);

		auto* el = new ELine(ServerInstance->Time(), duration, user->nick, reason, ih.first, ih.second);
		el->local = local;

		if (ServerInstance->XLines->AddLine(el, user))
		{
			if (!duration)
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a permanent {}E-line on {}: {}",
					user->nick, el->local ? "local " : "", target, el->reason);
			}
			else
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a timed {}E-line on {}, expires in {} (on {}): {}",
					user->nick, el->local ? "local " : "", target, Duration::ToLongString(duration),
					Time::FromNow(duration), el->reason);
			}
		}
		else
		{
			delete el;
			user->WriteNotice("*** E-line for " + target + " already exists.");
		}
	}
	else
	{
		std::string reason;

		if (ServerInstance->XLines->DelLine(target, "E", reason, user))
		{
			ServerInstance->SNO.WriteToSnoMask('x', "{} removed E-line on {}: {}", user->nick, target, reason);
		}
		else
		{
			user->WriteNotice("*** E-line " + target + " not found on the list.");
		}
	}

	return CmdResult::SUCCESS;
}
