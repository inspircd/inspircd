/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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
#include "modules/extban.h"

class ModuleNoKicks final
	: public Module
{
private:
	ExtBan::Acting extban;
	SimpleChannelMode nk;

public:
	ModuleNoKicks()
		: Module(VF_VENDOR, "Adds channel mode Q (nokick) which prevents privileged users from using the /KICK command.")
		, extban(this, "nokick", 'Q')
		, nk(this, "nokick", 'Q')
	{
	}

	ModResult OnUserPreKick(User* source, Membership* memb, const std::string& reason) override
	{
		bool modeset = memb->chan->IsModeSet(nk);
		if (!extban.GetStatus(source, memb->chan).check(!modeset))
		{
			// Can't kick with Q in place, not even opers with override, and founders
			source->WriteNumeric(ERR_RESTRICTED, memb->chan->name, INSP_FORMAT("Can't kick user {} from channel ({})",
				memb->user->nick, modeset ? "+Q is set" : "you're extbanned"));
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleNoKicks)
