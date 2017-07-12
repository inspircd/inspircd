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

class ModuleMLock : public Module
{
	StringExtItem mlock;

 public:
	ModuleMLock()
		: mlock("mlock", ExtensionItem::EXT_CHANNEL, this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements the ability to have server-side MLOCK enforcement.", VF_VENDOR);
	}

	ModResult OnRawMode(User* source, Channel* channel, ModeHandler* mh, const std::string& parameter, bool adding) CXX11_OVERRIDE
	{
		if (!channel)
			return MOD_RES_PASSTHRU;

		if (!IS_LOCAL(source))
			return MOD_RES_PASSTHRU;

		std::string *mlock_str = mlock.get(channel);
		if (!mlock_str)
			return MOD_RES_PASSTHRU;

		const char mode = mh->GetModeChar();
		std::string::size_type p = mlock_str->find(mode);
		if (p != std::string::npos)
		{
			source->WriteNumeric(742, channel->name, mode, *mlock_str, "MODE cannot be set due to channel having an active MLOCK restriction policy");
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleMLock)
