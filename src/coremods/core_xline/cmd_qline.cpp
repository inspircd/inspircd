/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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
	: Command(parent, "QLINE", 1, 3)
{
	flags_needed = 'o';
	syntax = "<nick> [<duration> :<reason>]";
}

CmdResult CommandQline::Handle (const std::vector<std::string>& parameters, User *user)
{
	if (parameters.size() >= 3)
	{
		NickMatcher matcher;
		if (InsaneBan::MatchesEveryone(parameters[0], matcher, user, "Q", "nickmasks"))
			return CMD_FAILURE;

		if (parameters[0].find('@') != std::string::npos || parameters[0].find('!') != std::string::npos || parameters[0].find('.') != std::string::npos)
		{
			user->WriteNotice("*** A Q-Line only bans a nick pattern, not a nick!user@host pattern.");
			return CMD_FAILURE;
		}

		unsigned long duration = InspIRCd::Duration(parameters[1]);
		QLine* ql = new QLine(ServerInstance->Time(), duration, user->nick.c_str(), parameters[2].c_str(), parameters[0].c_str());
		if (ServerInstance->XLines->AddLine(ql,user))
		{
			if (!duration)
			{
				ServerInstance->SNO->WriteToSnoMask('x',"%s added permanent Q-line for %s: %s",user->nick.c_str(), parameters[0].c_str(), parameters[2].c_str());
			}
			else
			{
				time_t c_requires_crap = duration + ServerInstance->Time();
				std::string timestr = InspIRCd::TimeString(c_requires_crap);
				ServerInstance->SNO->WriteToSnoMask('x',"%s added timed Q-line for %s, expires on %s: %s",user->nick.c_str(),parameters[0].c_str(),
						timestr.c_str(), parameters[2].c_str());
			}
			ServerInstance->XLines->ApplyLines();
		}
		else
		{
			delete ql;
			user->WriteNotice("*** Q-Line for " + parameters[0] + " already exists");
		}
	}
	else
	{
		if (ServerInstance->XLines->DelLine(parameters[0].c_str(), "Q", user))
		{
			ServerInstance->SNO->WriteToSnoMask('x',"%s removed Q-line on %s",user->nick.c_str(),parameters[0].c_str());
		}
		else
		{
			user->WriteNotice("*** Q-Line " + parameters[0] + " not found in list, try /stats q.");
			return CMD_FAILURE;
		}
	}

	return CMD_SUCCESS;
}

bool CommandQline::NickMatcher::Check(User* user, const std::string& nick) const
{
	return InspIRCd::Match(user->nick, nick);
}
