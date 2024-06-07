/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2019-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Matt Smith <dz@inspircd.org>
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
#include "modules/extban.h"

class ModuleAllowInvite final
	: public Module
{
private:
	ExtBan::Acting extban;
	SimpleChannelMode ni;

public:
	ModuleAllowInvite()
		: Module(VF_VENDOR, "Adds channel mode A (allowinvite) which allows unprivileged users to use the /INVITE command and extended ban A: (blockinvite) which bans specific masks from using the /INVITE command.")
		, extban(this, "blockinvite", 'A')
		, ni(this, "allowinvite", 'A')
	{
	}

	ModResult OnUserPreInvite(User* user, User* dest, Channel* channel, time_t timeout) override
	{
		if (IS_LOCAL(user))
		{
			ModResult res = extban.GetStatus(user, channel);
			if (res == MOD_RES_DENY)
			{
				// Matching extban, explicitly deny /invite
				user->WriteNumeric(ERR_RESTRICTED, channel->name, "You are banned from using INVITE");
				return res;
			}
			if (channel->IsModeSet(ni) || res == MOD_RES_ALLOW)
			{
				// Explicitly allow /invite
				return MOD_RES_ALLOW;
			}
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleAllowInvite)
