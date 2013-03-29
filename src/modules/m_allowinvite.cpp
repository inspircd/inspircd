/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

/* $ModDesc: Provides support for channel mode +A, allowing /invite freely on a channel and extban A to deny specific users it */

class AllowInvite : public SimpleChannelModeHandler
{
 public:
	AllowInvite(Module* Creator) : SimpleChannelModeHandler(Creator, "allowinvite", 'A') { }
};

class ModuleAllowInvite : public Module
{
	AllowInvite ni;
 public:

	ModuleAllowInvite() : ni(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(ni);
		Implementation eventlist[] = { I_OnUserPreInvite, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('A');
	}

	virtual ModResult OnUserPreInvite(User* user,User* dest,Channel* channel, time_t timeout)
	{
		if (IS_LOCAL(user))
		{
			ModResult res = channel->GetExtBanStatus(user, 'A');
			if (res == MOD_RES_DENY)
			{
				// Matching extban, explicitly deny /invite
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You are banned from using INVITE", user->nick.c_str(), channel->name.c_str());
				return res;
			}
			if (channel->IsModeSet('A') || res == MOD_RES_ALLOW)
			{
				// Explicitly allow /invite
				return MOD_RES_ALLOW;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	virtual ~ModuleAllowInvite()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for channel mode +A, allowing /invite freely on a channel and extban A to deny specific users it",VF_VENDOR);
	}
};

MODULE_INIT(ModuleAllowInvite)
