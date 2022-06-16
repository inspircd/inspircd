/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2015, 2017, 2019-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005-2006, 2010 Craig Edwards <brain@inspircd.org>
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

class ModuleRestrictMsg
	: public Module
	, public CTCTags::EventListener
{
 private:
	ModResult HandleMessage(User* user, const MessageTarget& target)
	{
		if ((target.type == MessageTarget::TYPE_USER) && (IS_LOCAL(user)))
		{
			User* u = target.Get<User>();

			// message allowed if:
			// (1) the sender is opered
			// (2) the recipient is opered
			// (3) the recipient is on a ulined server
			// anything else, blocked.
			if (u->IsOper() || user->IsOper() || u->server->IsULine())
				return MOD_RES_PASSTHRU;

			user->WriteNumeric(Numerics::CannotSendTo(u, "You cannot send messages to this user."));
			return MOD_RES_DENY;
		}

		// however, we must allow channel messages...
		return MOD_RES_PASSTHRU;
	}

 public:
	ModuleRestrictMsg()
		: CTCTags::EventListener(this)
	{
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		return HandleMessage(user, target);
	}

	ModResult OnUserPreTagMessage(User* user, const MessageTarget& target, CTCTags::TagMessageDetails& details) CXX11_OVERRIDE
	{
		return HandleMessage(user, target);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Prevents users who are not server operators from messaging each other.", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRestrictMsg)
