/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2004, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/** Handles user mode +B
 */
class BotMode : public SimpleUserModeHandler
{
 public:
	BotMode(Module* Creator) : SimpleUserModeHandler(Creator, "bot", 'B') { }
};

class ModuleBotMode : public Module, public Whois::EventListener
{
	BotMode bm;
 public:
	ModuleBotMode()
		: Whois::EventListener(this)
		, bm(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides user mode +B to mark the user as a bot",VF_VENDOR);
	}

	void OnWhois(Whois::Context& whois) CXX11_OVERRIDE
	{
		if (whois.GetTarget()->IsModeSet(bm))
		{
			whois.SendLine(335, ":is a bot on " + ServerInstance->Config->Network);
		}
	}
};

MODULE_INIT(ModuleBotMode)
