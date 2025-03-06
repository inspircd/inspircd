/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004 Craig Edwards <brain@inspircd.org>
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

class ModuleNoNickChange final
	: public Module
{
private:
	CheckExemption::EventProvider exemptionprov;
	ExtBan::Acting extban;
	SimpleChannelMode nn;

public:
	ModuleNoNickChange()
		: Module(VF_VENDOR, "Adds channel mode N (nonick) which prevents users from changing their nickname whilst in the channel.")
		, exemptionprov(this)
		, extban(this, "nonick", 'N')
		, nn(this, "nonick", 'N')
	{
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) override
	{
		for (const auto* memb : user->chans)
		{
			ModResult res = exemptionprov.Check(user, memb->chan, "nonick");
			if (res == MOD_RES_ALLOW)
				continue;

			if (user->HasPrivPermission("channels/ignore-nonicks"))
				continue;

			bool modeset = memb->chan->IsModeSet(nn);
			if (!extban.GetStatus(user, memb->chan).check(!modeset))
			{
				user->WriteNumeric(ERR_CANTCHANGENICK, INSP_FORMAT("Can't change nickname while on {} ({})", memb->chan->name,
					modeset ? INSP_FORMAT("+{} is set", nn.GetModeChar()) : "you're extbanned"));
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleNoNickChange)
