/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018, 2020, 2022-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@gmail.com>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

CommandQline::CommandQline(Module* parent)
	: Command(parent, "QLINE", 1, 4)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<nickmask>[,<nickmask>]+ [[<duration>] [LOCAL] :<reason>]" };
}

CmdResult CommandQline::Handle(User* user, const Params& parameters)
{
	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CmdResult::SUCCESS;

	if (parameters.size() > 1)
	{
		NickMatcher matcher;
		if (InsaneBan::MatchesEveryone(parameters[0], matcher, user, 'Q', "nickmasks"))
			return CmdResult::FAILURE;

		if (parameters[0].find('@') != std::string::npos || parameters[0].find('!') != std::string::npos || parameters[0].find('.') != std::string::npos)
		{
			user->WriteNotice("*** A Q-line only bans a nick pattern, not a nick!user@host pattern.");
			return CmdResult::FAILURE;
		}

		unsigned long duration = 0;
		if (parameters.size() > 2 && !Duration::TryFrom(parameters[1], duration))
		{
			user->WriteNotice("*** Invalid duration for Q-line.");
			return CmdResult::FAILURE;
		}

		auto local = false;
		auto reason = parameters.back();
		if (parameters.size() > 3 && irc::equals(parameters[2], "LOCAL"))
			local = true;
		else
			reason.insert(0, " ").insert(0, parameters[2]);

		auto* ql = new QLine(ServerInstance->Time(), duration, user->nick, reason, parameters[0]);
		ql->local = local;

		if (ServerInstance->XLines->AddLine(ql, user))
		{
			if (!duration)
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a permanent {}Q-line on {}: {}",
					user->nick, ql->local ? "local " : "", parameters[0], ql->reason);
			}
			else
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a timed {}Q-line on {}, expires in {} (on {}): {}",
					user->nick, ql->local ? "local " : "", parameters[0], Duration::ToLongString(duration),
					Time::FromNow(duration), ql->reason);
			}
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete ql;
			user->WriteNotice("*** Q-line for " + parameters[0] + " already exists.");
		}
	}
	else
	{
		std::string reason;

		if (ServerInstance->XLines->DelLine(parameters[0], "Q", reason, user))
		{
			ServerInstance->SNO.WriteToSnoMask('x', "{} removed Q-line on {}: {}", user->nick, parameters[0], reason);
		}
		else
		{
			user->WriteNotice("*** Q-line " + parameters[0] + " not found on the list.");
			return CmdResult::FAILURE;
		}
	}

	return CmdResult::SUCCESS;
}

bool CommandQline::NickMatcher::Check(User* user, const std::string& nick) const
{
	return InspIRCd::Match(user->nick, nick);
}
