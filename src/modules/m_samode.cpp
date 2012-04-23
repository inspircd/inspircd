/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2005 Craig Edwards <craigedwards@brainbox.cc>
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


/* $ModDesc: Provides more advanced UnrealIRCd SAMODE command */

#include "inspircd.h"

/** Handle /SAMODE
 */
class CommandSamode : public Command
{
 public:
	CommandSamode (InspIRCd* Instance) : Command(Instance,"SAMODE", "o", 2, false, 0)
	{
		this->source = "m_samode.so";
		syntax = "<target> <modes> {<mode-parameters>}";
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		/*
		 * Handles an SAMODE request. Notifies all +s users.
	 	 */
		ServerInstance->SendMode(parameters, ServerInstance->FakeClient);

		if (ServerInstance->Modes->GetLastParse().length())
		{
			ServerInstance->SNO->WriteToSnoMask('a', std::string(user->nick) + " used SAMODE: " + ServerInstance->Modes->GetLastParse());
			ServerInstance->PI->SendSNONotice("A", user->nick + " used SAMODE: " + ServerInstance->Modes->GetLastParse());

			std::string channel = parameters[0];
			ServerInstance->PI->SendMode(channel, ServerInstance->Modes->GetLastParseParams(), ServerInstance->Modes->GetLastParseTranslate());

			return CMD_LOCALONLY;
		}
		else
		{
			user->WriteServ("NOTICE %s :*** Invalid SAMODE sequence.", user->nick.c_str());
		}

		return CMD_FAILURE;
	}
};

class ModuleSaMode : public Module
{
	CommandSamode*	mycommand;
 public:
	ModuleSaMode(InspIRCd* Me)
		: Module(Me)
	{

		mycommand = new CommandSamode(ServerInstance);
		ServerInstance->AddCommand(mycommand);

	}

	virtual ~ModuleSaMode()
	{
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleSaMode)
