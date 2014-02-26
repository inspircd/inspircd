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


// Globops module by C.J.Edwards

#include "inspircd.h"

/** Handle /GLOBOPS
 */
class CommandGlobops : public Command
{
	Snomask& globops;

 public:
	CommandGlobops(Module* Creator, Snomask& Globops) : Command(Creator,"GLOBOPS", 1,1), globops(Globops)
	{
		flags_needed = 'o'; syntax = "<any-text>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{
		SnomaskManager::Write(SNO_REMOTE | SNO_BROADCAST, globops, "From " + user->nick + ": " + parameters[0]);
		return CMD_SUCCESS;
	}
};

class ModuleGlobops : public Module
{
	Snomask globops;
	CommandGlobops cmd;

 public:
	ModuleGlobops() : globops("GLOBOPS"), cmd(this, globops) {}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for GLOBOPS, and the snomask GLOBOPS", VF_VENDOR);
	}
};

MODULE_INIT(ModuleGlobops)
