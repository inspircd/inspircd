/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

class ModulePrivacyMode : public Module
{
	SimpleUserModeHandler pm;
 public:
	ModulePrivacyMode()
		: pm(this, "deaf_commonchan", 'c')
	{
	}

	Version GetVersion() override
	{
		return Version("Adds user mode +c, which if set, users must be on a common channel with you to private message you", VF_VENDOR);
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) override
	{
		if (target.type == MessageTarget::TYPE_USER)
		{
			User* t = target.Get<User>();
			if (!user->IsOper() && (t->IsModeSet(pm)) && (!user->server->IsULine()) && !user->SharesChannelWith(t))
			{
				user->WriteNumeric(ERR_CANTSENDTOUSER, t->nick, "You are not permitted to send private messages to this user (+c set)");
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModulePrivacyMode)
