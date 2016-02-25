/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/** Handle /CLONES
 */
class CommandClones : public Command
{
 public:
 	CommandClones(Module* Creator) : Command(Creator,"CLONES", 1)
	{
		flags_needed = 'o'; syntax = "<limit>";
	}

	CmdResult Handle (const std::vector<std::string> &parameters, User *user)
	{

		std::string clonesstr = "CLONES ";

		unsigned long limit = atoi(parameters[0].c_str());

		/*
		 * Syntax of a /clones reply:
		 *  :server.name 304 target :CLONES START
		 *  :server.name 304 target :CLONES <count> <ip>
		 *  :server.name 304 target :CLONES END
		 */

		user->WriteNumeric(304, clonesstr + "START");

		/* hostname or other */
		const UserManager::CloneMap& clonemap = ServerInstance->Users->GetCloneMap();
		for (UserManager::CloneMap::const_iterator i = clonemap.begin(); i != clonemap.end(); ++i)
		{
			const UserManager::CloneCounts& counts = i->second;
			if (counts.global >= limit)
				user->WriteNumeric(304, clonesstr + ConvToStr(counts.global) + " " + i->first.str());
		}

		user->WriteNumeric(304, clonesstr + "END");

		return CMD_SUCCESS;
	}
};

class ModuleClones : public Module
{
	CommandClones cmd;
 public:
	ModuleClones() : cmd(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides the /CLONES command to retrieve information on clones.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleClones)
