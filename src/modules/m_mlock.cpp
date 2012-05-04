/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 William Pitcock <nenolod@dereferenced.org>
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

class ModuleMLock : public Module {
private:
	StringExtItem mlock;

public:
	ModuleMLock() : mlock("mlock", this) {};

	void init()
	{
		Implementation eventlist[] = { I_OnPreMode };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	Version GetVersion()
	{
		return Version("Implements the ability to have server-side MLOCK enforcement.", VF_VENDOR);
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnPreMode, PRIORITY_FIRST);
	}

	ModResult OnPreMode(User* source, User* dest, Channel* channel, const std::vector<std::string>& parameters)
	{
		if (!channel)
			return MOD_RES_PASSTHRU;

		std::string *mlock_str = mlock.get(channel);
		if (mlock_str->empty())
			return MOD_RES_PASSTHRU;

		for (const char *modes = parameters[1].c_str(); *modes; modes++)
		{
			if (mlock_str->find(*modes))
			{
				source->WriteNumeric(742, "%s %c %s :MODE cannot be set due to channel having an active MLOCK restriction policy",
						     channel->name.c_str(), *modes, mlock_str->c_str());
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

};

MODULE_INIT(ModuleMLock)
