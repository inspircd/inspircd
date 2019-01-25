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
#include "modules/whois.h"

enum
{
	// From UnrealIRCd.
	RPL_WHOISBOT = 335
};

class ModuleBotMode : public Module, public Whois::EventListener
{
	SimpleUserModeHandler bm;
 public:
	ModuleBotMode()
		: Whois::EventListener(this)
		, bm(this, "bot", 'B')
	{
	}

	Version GetVersion() override
	{
		return Version("Provides user mode +B to mark the user as a bot",VF_VENDOR);
	}

	void OnWhois(Whois::Context& whois) override
	{
		if (whois.GetTarget()->IsModeSet(bm))
		{
			whois.SendLine(RPL_WHOISBOT, "is a bot on " + ServerInstance->Config->Network);
		}
	}
};

MODULE_INIT(ModuleBotMode)
