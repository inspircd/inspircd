/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005 Craig Edwards <craigedwards@brainbox.cc>
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

enum
{
	// InspIRCd-specific.
	ERR_INVALIDIDLETIME = 948,
	RPL_IDLETIMESET = 944
};

/** Handle /SETIDLE
 */
class CommandSetidle : public SplitCommand
{
 public:
	CommandSetidle(Module* Creator) : SplitCommand(Creator,"SETIDLE", 1)
	{
		flags_needed = 'o'; syntax = "<duration>";
	}

	CmdResult HandleLocal(LocalUser* user, const Params& parameters) CXX11_OVERRIDE
	{
		unsigned long idle;
		if (!InspIRCd::Duration(parameters[0], idle))
		{
			user->WriteNumeric(ERR_INVALIDIDLETIME, "Invalid idle time.");
			return CMD_FAILURE;
		}
		user->idle_lastmsg = (ServerInstance->Time() - idle);
		// minor tweak - we cant have signon time shorter than our idle time!
		if (user->signon > user->idle_lastmsg)
			user->signon = user->idle_lastmsg;
		ServerInstance->SNO->WriteToSnoMask('a', user->nick+" used SETIDLE to set their idle time to "+ConvToStr(idle)+" seconds");
		user->WriteNumeric(RPL_IDLETIMESET, "Idle time set.");

		return CMD_SUCCESS;
	}
};


class ModuleSetIdle : public Module
{
	CommandSetidle cmd;
 public:
	ModuleSetIdle()
		: cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the SETIDLE command, allows opers to set their idle time", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSetIdle)
