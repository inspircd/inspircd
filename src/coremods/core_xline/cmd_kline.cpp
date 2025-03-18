/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018, 2020-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

CommandKline::CommandKline(Module* parent)
	: Command(parent, "KLINE", 1, 3)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<user@host>[,<user@host>]+ [[<duration>] :<reason>]" };
}

CmdResult CommandKline::Handle(User* user, const Params& parameters)
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
		if (InsaneBan::MatchesEveryone(ih.first + "@" + ih.second, matcher, user, 'K', "hostmasks"))
			return CmdResult::FAILURE;

		if (target.find('!') != std::string::npos)
		{
			user->WriteNotice("*** K-line cannot operate on nick!user@host masks.");
			return CmdResult::FAILURE;
		}

		unsigned long duration = 0;
		if (parameters.size() > 2 && !Duration::TryFrom(parameters[1], duration))
		{
			user->WriteNotice("*** Invalid duration for K-line.");
			return CmdResult::FAILURE;
		}

		auto* kl = new KLine(ServerInstance->Time(), duration, user->nick, parameters.back(), ih.first, ih.second);
		if (ServerInstance->XLines->AddLine(kl, user))
		{
			if (!duration)
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a permanent K-line on {}: {}",
					user->nick, target, kl->reason);
			}
			else
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a timed K-line on {}, expires in {} (on {}): {}",
					user->nick, target, Duration::ToLongString(duration), Time::FromNow(duration), kl->reason);
			}

			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete kl;
			user->WriteNotice("*** K-line for " + target + " already exists.");
		}
	}
	else
	{
		std::string reason;

		if (ServerInstance->XLines->DelLine(target, "K", reason, user))
		{
			ServerInstance->SNO.WriteToSnoMask('x', "{} removed K-line on {}: {}", user->nick, target, reason);
		}
		else
		{
			user->WriteNotice("*** K-line " + target + " not found on the list.");
		}
	}

	return CmdResult::SUCCESS;
}
