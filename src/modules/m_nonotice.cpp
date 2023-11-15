/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2017, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Shawn Smith <ShawnSmith0828@gmail.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006 Craig Edwards <brain@inspircd.org>
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
#include "modules/exemption.h"
#include "modules/extban.h"
#include "numerichelper.h"

class ModuleNoNotice final
	: public Module
{
private:
	ExtBan::Acting extban;
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelMode nt;

public:
	ModuleNoNotice()
		: Module(VF_VENDOR, "Adds channel mode T (nonotice) which allows channels to block messages sent with the /NOTICE command.")
		, extban(this, "nonotice", 'T')
		, exemptionprov(this)
		, nt(this, "nonotice", 'T')
	{
	}

	ModResult OnUserPreMessage(User* user, MessageTarget& target, MessageDetails& details) override
	{
		if ((details.type == MessageType::NOTICE) && (target.type == MessageTarget::TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = target.Get<Channel>();

			ModResult res = exemptionprov.Check(user, c, "nonotice");
			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			if (c->IsModeSet(nt))
			{
				user->WriteNumeric(Numerics::CannotSendTo(c, "notices", &nt));
				return MOD_RES_DENY;
			}

			if (extban.GetStatus(user, c) == MOD_RES_DENY)
			{
				user->WriteNumeric(Numerics::CannotSendTo(c, "notices", extban));
				return MOD_RES_DENY;
			}
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleNoNotice)
