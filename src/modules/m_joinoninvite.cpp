/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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

/*
 * Written by Łukasz "JustArchi" Domeradzki <JustArchi@JustArchi.net>, August 2014.
 * Should be used with caution, as it allows potential abuse by users
 * Perfect when INVITE command is banned and reserved only to net admins
 * Simple, yet powerful
 */

 /*
 * TODO:
 * Add an option to auto accept invites only by specific net admins?
 */

class ModuleJoinOnInvite : public Module
{
 public:
	ModuleJoinOnInvite()
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Forces user to join the channel on invite", VF_VENDOR);
	}

	void OnUserInvite(User* source,User* dest,Channel* channel, time_t timeout) CXX11_OVERRIDE
	{
		LocalUser* localuser = IS_LOCAL(dest);
		if (!localuser)
			return;

		Channel::JoinUser(localuser, channel->name);
	}
};

MODULE_INIT(ModuleJoinOnInvite)
