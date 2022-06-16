/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2008, 2010 Craig Edwards <brain@inspircd.org>
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
	flags_needed = 'o';
	syntax = "<user@host> [<duration> :<reason>]";
}

/** Handle /GLINE
 */
CmdResult CommandGline::Handle(User* user, const Params& parameters)
{
	std::string target = parameters[0];

	if (parameters.size() >= 3)
	{
		IdentHostPair ih;
		User* find = ServerInstance->FindNick(target);
		if ((find) && (find->registered == REG_ALL))
		{
			ih.first = find->GetBanIdent();
			ih.second = find->GetIPString();
			target = ih.first + "@" + ih.second;
		}
		else
			ih = ServerInstance->XLines->IdentSplit(target);

		if (ih.first.empty())
		{
			user->WriteNotice("*** Target not found.");
			return CMD_FAILURE;
		}

		InsaneBan::IPHostMatcher matcher;
		if (InsaneBan::MatchesEveryone(ih.first+"@"+ih.second, matcher, user, "G", "hostmasks"))
			return CMD_FAILURE;

		else if (target.find('!') != std::string::npos)
		{
			user->WriteNotice("*** G-line cannot operate on nick!user@host masks.");
			return CMD_FAILURE;
		}

		unsigned long duration;
		if (!InspIRCd::Duration(parameters[1], duration))
		{
			user->WriteNotice("*** Invalid duration for G-line.");
			return CMD_FAILURE;
		}
		GLine* gl = new GLine(ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), ih.first.c_str(), ih.second.c_str());
		if (ServerInstance->XLines->AddLine(gl, user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s added permanent G-line for %s: %s", user->nick.c_str(), target.c_str(), parameters[2].c_str());
			}
			else
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s added timed G-line for %s, expires in %s (on %s): %s",
					user->nick.c_str(), target.c_str(), InspIRCd::DurationString(duration).c_str(),
					InspIRCd::TimeString(ServerInstance->Time() + duration).c_str(), parameters[2].c_str());
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

		if (ServerInstance->XLines->DelLine(target.c_str(), "G", reason, user))
		{
			ServerInstance->SNO->WriteToSnoMask('x', "%s removed G-line on %s: %s", user->nick.c_str(), target.c_str(), reason.c_str());
		}
		else
		{
			user->WriteNotice("*** G-line " + target + " not found on the list.");
		}
	}

	return CMD_SUCCESS;
}
