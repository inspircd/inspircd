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

/* $ModDesc: Provides the /CLONES command to retrieve information on clones. */

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

		std::string clonesstr = "304 " + user->nick + " :CLONES";

		unsigned long limit = atoi(parameters[0].c_str());

		/*
		 * Syntax of a /clones reply:
		 *  :server.name 304 target :CLONES START
		 *  :server.name 304 target :CLONES <count> <ip>
		 *  :server.name 304 target :CLONES END
		 */

		user->WriteServ(clonesstr + " START");

		/* hostname or other */
		// XXX I really don't like marking global_clones public for this. at all. -- w00t
		for (clonemap::iterator x = ServerInstance->Users->global_clones.begin(); x != ServerInstance->Users->global_clones.end(); x++)
		{
			if (x->second >= limit)
				user->WriteServ(clonesstr + " "+ ConvToStr(x->second) + " " + x->first.str());
		}

		user->WriteServ(clonesstr + " END");

		return CMD_SUCCESS;
	}
};


class ModuleClones : public Module
{
 private:
	CommandClones cmd;
 public:
	ModuleClones() : cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
	}

	virtual ~ModuleClones()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides the /CLONES command to retrieve information on clones.", VF_VENDOR);
	}


};

MODULE_INIT(ModuleClones)
