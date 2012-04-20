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

/* $ModDesc: Provides support for channel mode +A, allowing /invite freely on a channel (and extban A to allow specific users it) */

class AllowInvite : public SimpleChannelModeHandler
{
 public:
	AllowInvite(InspIRCd* Instance) : SimpleChannelModeHandler(Instance, 'A') { }
};

class ModuleAllowInvite : public Module
{
	AllowInvite *ni;
 public:

	ModuleAllowInvite(InspIRCd* Me) : Module(Me)
	{
		ni = new AllowInvite(ServerInstance);
		if (!ServerInstance->Modes->AddMode(ni))
			throw ModuleException("Could not add new modes!");
		Implementation eventlist[] = { I_OnUserPreInvite, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('A');
	}

	virtual int OnUserPreInvite(User* user,User* dest,Channel* channel, time_t timeout)
	{
		if (IS_LOCAL(user))
		{
			if (channel->GetExtBanStatus(user, 'A') == -1)
			{
				// Matching extban, explicitly deny /invite
				user->WriteNumeric(ERR_CHANOPRIVSNEEDED, "%s %s :You are banned from using INVITE", user->nick.c_str(), channel->name.c_str());
				return 1;
			}
			if (channel->IsModeSet('A') || channel->GetExtBanStatus(user, 'A') == 1)
			{
				// Explicitly allow /invite
				return -1;
			}
		}

		return 0;
	}

	virtual ~ModuleAllowInvite()
	{
		ServerInstance->Modes->DelMode(ni);
		delete ni;
	}

	virtual Version GetVersion()
	{
		return Version("$Id$",VF_COMMON|VF_VENDOR,API_VERSION);
	}
};

MODULE_INIT(ModuleAllowInvite)
