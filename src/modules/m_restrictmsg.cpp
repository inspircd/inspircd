/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2006 Craig Edwards <brain@inspircd.org>
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

class ModuleRestrictMsg final
	: public Module
	, public CTCTags::EventListener
{
private:
	static ModResult HandleMessage(User* user, const MessageTarget& target)
	{
		if ((target.type == MessageTarget::TYPE_USER) && (IS_LOCAL(user)))
		{
			User* u = target.Get<User>();

			// message allowed if:
			// (1) the sender is opered
			// (2) the recipient is opered
			// (3) the recipient is on a services server
			// anything else, blocked.
			if (u->IsOper() || user->IsOper() || u->server->IsService())
				return MOD_RES_PASSTHRU;

			user->WriteNumeric(Numerics::CannotSendTo(u, "You cannot send messages to this user."));
			return MOD_RES_DENY;
		}

		// however, we must allow channel messages...
		return MOD_RES_PASSTHRU;
	}

public:
	ModuleRestrictMsg()
		: Module(VF_VENDOR, "Prevents users who are not server operators from messaging each other.")
		, CTCTags::EventListener(this)
	{
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

MODULE_INIT(ModuleRestrictMsg)
