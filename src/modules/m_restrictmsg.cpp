/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2005 Craig Edwards <craigedwards@brainbox.cc>
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

class ModuleRestrictMsg : public Module
{
 public:
	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if ((target_type == TYPE_USER) && (IS_LOCAL(user)))
		{
			User* u = (User*)dest;

			// message allowed if:
			// (1) the sender is opered
			// (2) the recipient is opered
			// (3) the recipient is on a ulined server
			// anything else, blocked.
			if (u->IsOper() || user->IsOper() || u->server->IsULine())
			{
				return MOD_RES_PASSTHRU;
			}
			user->WriteNumeric(ERR_CANTSENDTOUSER, "%s :You are not permitted to send private messages to this user", u->nick.c_str());
			return MOD_RES_DENY;
		}

		// however, we must allow channel messages...
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Forbids users from messaging each other. Users may still message opers and opers may message other opers.",VF_VENDOR);
	}
};

MODULE_INIT(ModuleRestrictMsg)
