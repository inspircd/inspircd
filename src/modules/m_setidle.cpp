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
	CommandSetidle (InspIRCd* Instance) : Command(Instance,"SETIDLE", "o", 1)
	{
		this->source = "m_setidle.so";
		syntax = "<duration>";
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
		ServerInstance->SNO->WriteToSnoMask('a', std::string(user->nick)+" used SETIDLE to set their idle time to "+ConvToStr(idle)+" seconds");
		user->WriteNumeric(944, "%s :Idle time set.",user->nick.c_str());

		return CMD_LOCALONLY;
	}
};


class ModuleSetIdle : public Module
{
	CommandSetidle*	mycommand;
 public:
	ModuleSetIdle(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSetidle(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSetIdle()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSetIdle)
