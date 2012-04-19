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

/* $ModDesc: Provides support for unreal-style umode +B */

/** Handles user mode +B
 */
class BotMode : public SimpleUserModeHandler
{
 public:
	BotMode(Module* Creator) : SimpleUserModeHandler(Creator, "bot", 'B') { }
};

class ModuleBotMode : public Module
{
	BotMode bm;
 public:
	ModuleBotMode() : bm(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(bm);
		Implementation eventlist[] = { I_OnWhois };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}


	virtual ~ModuleBotMode()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for unreal-style umode +B",VF_VENDOR);
	}

	virtual void OnWhois(User* src, User* dst)
	{
		if (dst->IsModeSet('B'))
		{
			ServerInstance->SendWhoisLine(src, dst, 335, std::string(src->nick)+" "+std::string(dst->nick)+" :is a bot on "+ServerInstance->Config->Network);
		}
	}

};


MODULE_INIT(ModuleBotMode)
