/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

/* $ModDesc: Provides channel mode +Q to prevent kicks on the channel. */

class NoKicks : public SimpleChannelModeHandler
{
 public:
	NoKicks(Module* Creator) : SimpleChannelModeHandler(Creator, "nokick", 'Q') { }
};

class ModuleNoKicks : public Module
{
	NoKicks nk;

 public:
	ModuleNoKicks()
		: nk(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(nk);
		Implementation eventlist[] = { I_OnUserPreKick, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('Q');
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string &reason)
	{
		if (!memb->chan->GetExtBanStatus(source, 'Q').check(!memb->chan->IsModeSet('Q')))
		{
			// Can't kick with Q in place, not even opers with override, and founders
			source->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :Can't kick user %s from channel (+Q set)",source->nick.c_str(), memb->chan->name.c_str(), memb->user->nick.c_str());
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	~ModuleNoKicks()
	{
	}

	Version GetVersion()
	{
		return Version("Provides channel mode +Q to prevent kicks on the channel.", VF_VENDOR);
	}
};


MODULE_INIT(ModuleNoKicks)
