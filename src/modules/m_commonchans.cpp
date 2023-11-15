/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017, 2019-2020, 2022 Sadie Powell <sadie@witchery.services>
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
#include "numerichelper.h"

class ModuleCommonChans final
	: public Module
	, public CTCTags::EventListener
{
private:
	SimpleUserMode mode;

	bool IsExempt(User* source, User* target)
	{
		if (!target->IsModeSet(mode) || source->SharesChannelWith(target))
			return true; // Target doesn't have mode set or shares a common channel.

		if (source->HasPrivPermission("users/ignore-commonchans") || source->server->IsService())
			return true; // Source is an oper or a service.

		return false;
	}

	ModResult HandleMessage(User* user, const MessageTarget& target)
	{
		if (target.type != MessageTarget::TYPE_USER)
			return MOD_RES_PASSTHRU;

		User* targetuser = target.Get<User>();
		if (IsExempt(user, targetuser))
			return MOD_RES_PASSTHRU;

		user->WriteNumeric(Numerics::CannotSendTo(targetuser, "messages", &mode));
		return MOD_RES_DENY;
	}

public:
	ModuleCommonChans()
		: Module(VF_VENDOR, "Adds user mode c (deaf_commonchan) which requires users to have a common channel before they can privately message each other.")
		, CTCTags::EventListener(this)
		, mode(this, "deaf_commonchan", 'c')
	{
	}

	ModResult OnUserPreInvite(User* source, User* dest, Channel* channel, time_t timeout) override
	{
		if (IsExempt(source, dest))
			return MOD_RES_PASSTHRU;

		source->WriteNumeric(Numerics::CannotSendTo(dest, "invites", &mode));
		return MOD_RES_DENY;
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreTagMessage(User* user, MessageTarget& target, CTCTags::TagMessageDetails& details) override
	{
		return HandleMessage(user, target);
	}
};

MODULE_INIT(ModuleCommonChans)
