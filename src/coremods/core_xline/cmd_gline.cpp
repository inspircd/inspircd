/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018, 2020-2023 Sadie Powell <sadie@witchery.services>
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

CommandGline::CommandGline(Module* parent)
	: Command(parent, "GLINE", 1, 3)
{
	access_needed = CmdAccess::OPERATOR;
	syntax = { "<user@host>[,<user@host>]+ [<duration> :<reason>]" };
}

CmdResult CommandGline::Handle(User* user, const Params& parameters)
{
	if (CommandParser::LoopCall(user, this, parameters, 0))
		return CmdResult::SUCCESS;

	std::string target = parameters[0];
	if (parameters.size() >= 3)
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
		if (InsaneBan::MatchesEveryone(ih.first + "@" + ih.second, matcher, user, 'G', "hostmasks"))
			return CmdResult::FAILURE;

		else if (target.find('!') != std::string::npos)
		{
			user->WriteNotice("*** G-line cannot operate on nick!user@host masks.");
			return CmdResult::FAILURE;
		}

		unsigned long duration;
		if (!Duration::TryFrom(parameters[1], duration))
		{
			user->WriteNotice("*** Invalid duration for G-line.");
			return CmdResult::FAILURE;
		}

		auto* gl = new GLine(ServerInstance->Time(), duration, user->nick, parameters[2], ih.first, ih.second);
		if (ServerInstance->XLines->AddLine(gl, user))
		{
			if (!duration)
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a permanent G-line on {}: {}", user->nick, target, parameters[2]);
			}
			else
			{
				ServerInstance->SNO.WriteToSnoMask('x', "{} added a timed G-line on {}, expires in {} (on {}): {}",
					user->nick, target, Duration::ToString(duration),
					Time::FromNow(duration), parameters[2]);
			}

			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete gl;
			user->WriteNotice("** G-line for " + target + " already exists.");
		}

	}
	else
	{
		std::string reason;

		if (ServerInstance->XLines->DelLine(target, "G", reason, user))
		{
			ServerInstance->SNO.WriteToSnoMask('x', "{} removed G-line on {}: {}", user->nick, target, reason);
		}
		else
		{
			user->WriteNotice("*** G-line " + target + " not found on the list.");
		}
	}

	return CmdResult::SUCCESS;
}
