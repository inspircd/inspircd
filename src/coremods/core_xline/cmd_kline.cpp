/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
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
	flags_needed = 'o';
	syntax = "<user@host> [<duration> :<reason>]";
}

/** Handle /KLINE
 */
CmdResult CommandKline::Handle(User* user, const Params& parameters)
{
	std::string target = parameters[0];

	if (parameters.size() >= 3)
	{
		IdentHostPair ih;
		User* find = ServerInstance->FindNick(target);
		if ((find) && (find->registered == REG_ALL))
		{
			ih.first = "*";
			ih.second = find->GetIPString();
			target = std::string("*@") + find->GetIPString();
		}
		else
			ih = ServerInstance->XLines->IdentSplit(target);

		if (ih.first.empty())
		{
			user->WriteNotice("*** Target not found.");
			return CMD_FAILURE;
		}

		InsaneBan::IPHostMatcher matcher;
		if (InsaneBan::MatchesEveryone(ih.first+"@"+ih.second, matcher, user, "K", "hostmasks"))
			return CMD_FAILURE;

		if (target.find('!') != std::string::npos)
		{
			user->WriteNotice("*** K-line cannot operate on nick!user@host masks.");
			return CMD_FAILURE;
		}

		unsigned long duration;
		if (!InspIRCd::Duration(parameters[1], duration))
		{
			user->WriteNotice("*** Invalid duration for K-line.");
			return CMD_FAILURE;
		}
		KLine* kl = new KLine(ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), ih.first.c_str(), ih.second.c_str());
		if (ServerInstance->XLines->AddLine(kl,user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s added permanent K-line for %s: %s", user->nick.c_str(), target.c_str(), parameters[2].c_str());
			}
			else
			{
				ServerInstance->SNO->WriteToSnoMask('x', "%s added timed K-line for %s, expires in %s (on %s): %s",
					user->nick.c_str(), target.c_str(), InspIRCd::DurationString(duration).c_str(),
					InspIRCd::TimeString(ServerInstance->Time() + duration).c_str(), parameters[2].c_str());
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

		if (ServerInstance->XLines->DelLine(target.c_str(), "K", reason, user))
		{
			ServerInstance->SNO->WriteToSnoMask('x', "%s removed K-line on %s: %s", user->nick.c_str(), target.c_str(), reason.c_str());
		}
		else
		{
			user->WriteNotice("*** K-line " + target + " not found on the list.");
		}
	}

	return CMD_SUCCESS;
}
