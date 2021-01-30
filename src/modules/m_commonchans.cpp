/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017, 2019-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Craig Edwards <brain@inspircd.org>
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
	: public Module
	, public CTCTags::EventListener
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

		if (user->HasPrivPermission("users/ignore-commonchans") || user->server->IsService())
			return MOD_RES_PASSTHRU;

		user->WriteNumeric(Numerics::CannotSendTo(targuser, "messages", &mode));
		return MOD_RES_DENY;
	}

 public:
	ModuleCommonChans()
		: Module(VF_VENDOR, "Adds user mode c (deaf_commonchan) which requires users to have a common channel before they can privately message each other.")
		, CTCTags::EventListener(this)
		, mode(this, "deaf_commonchan", 'c')
	{
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) override
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreTagMessage(User* user, const MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		return HandleMessage(user, target);
	}
};

MODULE_INIT(ModuleCommonChans)
