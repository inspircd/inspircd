/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2004-2006 Craig Edwards <craigedwards@brainbox.cc>
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


// Globops and snomask +g module by C.J.Edwards

#include "inspircd.h"

/* $ModDesc: Provides support for GLOBOPS and snomask +g */

/** Handle /GLOBOPS
 */
class CommandGlobops : public Command
{
 public:
	CommandGlobops(Module* Creator) : Command(Creator,"GLOBOPS", 1,1)
	{
		flags_needed = 'o'; syntax = "<any-text>";
		TRANSLATE2(TR_TEXT, TR_END);
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		ServerInstance->SNO->WriteGlobalSno('g', "From " + user->nick + ": " + parameters[0]);
		return CMD_SUCCESS;
	}
};

class ModuleGlobops : public Module
{
	CommandGlobops cmd;
 public:
	ModuleGlobops() : cmd(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->SNO->EnableSnomask('g',"GLOBOPS");
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for GLOBOPS and snomask +g", VF_VENDOR);
	}

};

MODULE_INIT(ModuleGlobops)
