/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

/* $ModDesc: Provides support for unreal-style channel mode +Q */

class NoKicks : public SimpleChannelModeHandler
{
 public:
	NoKicks(Module* Creator) : SimpleChannelModeHandler(Creator, "nokick", 'Q') { fixed_letter = false; }
};

class ModuleNoKicks : public Module
{
	NoKicks nk;

 public:
	ModuleNoKicks() : nk(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(nk);
		Implementation eventlist[] = { I_OnPermissionCheck, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('Q');
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (perm.name == "kick" && !perm.chan->GetExtBanStatus(perm.source, 'Q').check(!perm.chan->IsModeSet(&nk)))
		{
			perm.ErrorNumeric(ERR_CHANOPRIVSNEEDED, "%s :Can't kick in channel (+Q set)", perm.chan->name.c_str());
			perm.result = MOD_RES_DENY;
		}
	}

	~ModuleNoKicks()
	{
	}

	Version GetVersion()
	{
		return Version("Provides support for unreal-style channel mode +Q", VF_VENDOR);
	}
};


MODULE_INIT(ModuleNoKicks)
