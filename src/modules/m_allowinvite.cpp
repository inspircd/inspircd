/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Provides support for channel mode +A, allowing /invite freely on a channel (and extban A to allow specific users it) */

class AllowInvite : public SimpleChannelModeHandler
{
 public:
	AllowInvite(Module* Creator) : SimpleChannelModeHandler(Creator, "allowinvite", 'A') { fixed_letter = false; }
};

class ModuleAllowInvite : public Module
{
	AllowInvite ni;
 public:

	ModuleAllowInvite() : ni(this) {}

	void init()
	{
		ServerInstance->Modules->AddService(ni);
		Implementation eventlist[] = { I_OnPermissionCheck, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('A');
	}

	void OnPermissionCheck(PermissionData& perm)
	{
		if (perm.name != "invite")
			return;

		ModResult res = perm.chan->GetExtBanStatus(perm.source, 'A');
		if (res == MOD_RES_DENY)
		{
			// Matching extban, explicitly deny /invite
			perm.result = res;
			perm.SetReason(":%s %d %s %s :You are banned from using INVITE", ServerInstance->Config->ServerName.c_str(),
				ERR_CHANOPRIVSNEEDED, perm.source->nick.c_str(), perm.chan->name.c_str());
		}
		else if (perm.chan->IsModeSet(&ni) || res == MOD_RES_ALLOW)
		{
			perm.result = MOD_RES_ALLOW;
		}
	}

	virtual ~ModuleAllowInvite()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for channel mode +A, allowing /invite freely on a channel (and extban A to allow specific users it)",VF_VENDOR);
	}
};

MODULE_INIT(ModuleAllowInvite)
