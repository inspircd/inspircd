/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005 Craig Edwards <brain@inspircd.org>
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
#include "modules/ircv3_replies.h"
#include "timeutils.h"

class CommandSetidle final
	: public SplitCommand
{
private:
	IRCv3::Replies::Fail failrpl;
	IRCv3::Replies::Fail noterpl;
	IRCv3::Replies::CapReference stdrplcap;

public:
	CommandSetidle(Module* Creator)
		: SplitCommand(Creator, "SETIDLE", 1)
		, failrpl(Creator)
		, noterpl(Creator)
		, stdrplcap(Creator)
	{
		access_needed = CmdAccess::OPERATOR;
		syntax = { "<duration>" };
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) override
	{
		unsigned long idle;
		if (!Duration::TryFrom(parameters[0], idle))
		{
			failrpl.SendIfCap(user, stdrplcap, this, "INVALID_IDLE_TIME", parameters[0], "Invalid idle time.");
			return CmdResult::FAILURE;
		}

		user->idle_lastmsg = (ServerInstance->Time() - idle);
		// minor tweak - we cant have signon time shorter than our idle time!
		if (user->signon > user->idle_lastmsg)
			user->signon = user->idle_lastmsg;

		ServerInstance->SNO.WriteToSnoMask('a', "{} used SETIDLE to set their idle time to {}", user->nick, Duration::ToLongString(idle));
		noterpl.SendIfCap(user, stdrplcap, this, "IDLE_TIME_SET", user->idle_lastmsg, "Idle time set.");
		return CmdResult::SUCCESS;
	}
};

class ModuleSetIdle final
	: public Module
{
private:
	CommandSetidle cmd;

public:
	ModuleSetIdle()
		: Module(VF_VENDOR, "Adds the /SETIDLE command which allows server operators to change their idle time.")
		, cmd(this)
	{
	}
};

MODULE_INIT(ModuleSetIdle)
