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


/* $ModDesc: Implements the ability to have server-side MLOCK enforcement. */

#include "inspircd.h"

class ModuleMLock : public Module
{
private:
	StringExtItem mlock;

public:
	ModuleMLock() : mlock(EXTENSIBLE_CHANNEL, "mlock", this) {};

	void init()
	{
		ServerInstance->Modules->Attach(I_OnPreMode, this);
		ServerInstance->Extensions.Register(&this->mlock);
	}

	Version GetVersion()
	{
		return Version("Implements the ability to have server-side MLOCK enforcement.", VF_VENDOR);
	}

	void Prioritize()
	{
		ServerInstance->Modules->SetPriority(this, I_OnPreMode, PRIORITY_FIRST);
	}

	ModResult OnPreMode(User* source, Extensible *e, irc::modestacker &ms)
	{
		Channel *ch = dynamic_cast<Channel *>(e);

		if (!ch)
			return MOD_RES_PASSTHRU;

		if (!IS_LOCAL(source))
			return MOD_RES_PASSTHRU;

		std::string *mlock_str = mlock.get(e);
		if (!mlock_str || mlock_str->empty())
			return MOD_RES_PASSTHRU;

		for(std::vector<irc::modechange>::iterator iter = ms.sequence.begin(); iter != ms.sequence.end(); ++iter)
		{
			ModeHandler *mh = ServerInstance->Modes->FindMode(iter->mode);
			char modechar = mh->GetModeChar();

			if (mlock_str->find(modechar) != std::string::npos)
			{
				source->WriteNumeric(742, "%s %c %s :MODE cannot be set due to channel having an active MLOCK restriction policy",
						     ch->name.c_str(), modechar, mlock_str->c_str());
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

};

MODULE_INIT(ModuleMLock)
