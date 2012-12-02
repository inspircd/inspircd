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

/* $ModDesc: Allows opers to set their idle time */

/** Handle /SETIDLE
 */
class CommandSetidle : public Command
{
 public:
	CommandSetidle(Module* Creator) : Command(Creator,"SETIDLE", 1)
	{
		flags_needed = 'o'; syntax = "<duration>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		time_t idle = ServerInstance->Duration(parameters[0]);
		if (idle < 1)
		{
			user->WriteNumeric(948, "%s :Invalid idle time.",user->nick.c_str());
			return CMD_FAILURE;
		}
		user->idle_lastmsg = (ServerInstance->Time() - idle);
		// minor tweak - we cant have signon time shorter than our idle time!
		if (user->signon > user->idle_lastmsg)
			user->signon = user->idle_lastmsg;
		ServerInstance->SNO->WriteToSnoMask('a', user->nick+" used SETIDLE to set their idle time to "+ConvToStr(idle)+" seconds");
		user->WriteNumeric(944, "%s :Idle time set.",user->nick.c_str());

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

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	virtual ~ModuleSetIdle()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Allows opers to set their idle time", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSetIdle)
