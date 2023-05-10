/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

CommandEline::CommandEline(Module* parent)
	: Command(parent, "ELINE", 1, 3)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<user@host>[,<user@host>]+ [<duration> :<reason>]" };
}

CmdResult CommandEline::Handle(User* user, const Params& parameters)
{
	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CmdResult::SUCCESS;

	std::string target = parameters[0];
	if (parameters.size() >= 3)
	{
		IdentHostPair ih;
		auto* find = ServerInstance->Users.Find(target, true);
		if (find)
		{
			ih.first = find->GetBanIdent();
			ih.second = find->GetAddress();
			target = ih.first + "@" + ih.second;
		}
		else
			ih = XLineManager::IdentSplit(target);

		if (ih.first.empty())
		{
			user->WriteNotice("*** Target not found.");
			return CmdResult::FAILURE;
		}

		InsaneBan::IPHostMatcher matcher;
		if (InsaneBan::MatchesEveryone(ih.first + "@" + ih.second, matcher, user, 'E', "hostmasks"))
			return CmdResult::FAILURE;

		unsigned long duration;
		if (!Duration::TryFrom(parameters[1], duration))
		{
			user->WriteNotice("*** Invalid duration for E-line.");
			return CmdResult::FAILURE;
		}

		auto* el = new ELine(ServerInstance->Time(), duration, user->nick, parameters[2], ih.first, ih.second);
		if (ServerInstance->XLines->AddLine(el, user))
		{
			if (!duration)
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a permanent E-line on {}: {}", user->nick, target, parameters[2]);
			}
			else
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a timed E-line on {}, expires in {} (on {}): {}",
					user->nick, target, Duration::ToString(duration),
					Time::ToString(ServerInstance->Time() + duration), parameters[2]);
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

		if (ServerInstance->XLines->DelLine(target.c_str(), "E", reason, user))
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
