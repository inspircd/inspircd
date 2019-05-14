/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Peter Powell <petpow@saberuk.com>
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
#include "modules/ctctags.h"

class ModuleCommonChans
	: public CTCTags::EventListener
	, public Module
{
 private:
	SimpleUserModeHandler mode;

	ModResult HandleMessage(User* user, const MessageTarget& target)
	{
		if (target.type != MessageTarget::TYPE_USER)
			return MOD_RES_PASSTHRU;

		User* targuser = target.Get<User>();
		if (!targuser->IsModeSet(mode) || user->SharesChannelWith(targuser))
			return MOD_RES_PASSTHRU;

		if (user->HasPrivPermission("users/ignore-commonchans") || user->server->IsULine())
			return MOD_RES_PASSTHRU;

		user->WriteNumeric(ERR_CANTSENDTOUSER, targuser->nick, "You are not permitted to send private messages to this user (+c is set)");
		return MOD_RES_DENY;
	}

 public:
	ModuleCommonChans()
		: CTCTags::EventListener(this)
		, mode(this, "deaf_commonchan", 'c')
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides user mode +c, requires users to share a common channel with you to private message you", VF_VENDOR);
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreTagMessage(User* user, const MessageTarget& target, CTCTags::TagMessageDetails& details) CXX11_OVERRIDE
	{
		return HandleMessage(user, target);
	}
};

MODULE_INIT(ModuleCommonChans)
