/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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


/*
 * DEVOICE module for InspIRCd
 *  Syntax: /DEVOICE <#chan>
 */

#include "inspircd.h"

/** Handle /DEVOICE
 */
class CommandDevoice : public Command
{
 public:
	CommandDevoice(Module* Creator) : Command(Creator,"DEVOICE", 1)
	{
		syntax = "<channel>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		std::vector<std::string> modes;
		modes.push_back(parameters[0]);
		modes.push_back("-v");
		modes.push_back(user->nick);

		ServerInstance->Parser.CallHandler("MODE", modes, ServerInstance->FakeClient);
		return CMD_SUCCESS;
	}
};

class ModuleDeVoice : public Module
{
	CommandDevoice cmd;
 public:
	ModuleDeVoice() : cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides voiced users with the ability to devoice themselves.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleDeVoice)
