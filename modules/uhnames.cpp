/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019-2021, 2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "modules/cap.h"
#include "modules/names.h"

class ModuleUHNames final
	: public Module
	, public Names::EventListener
{
private:
	Cap::Capability cap;

public:
	ModuleUHNames()
		: Module(VF_VENDOR, "Provides the IRCv3 userhost-in-names client capability.")
		, Names::EventListener(this)
		, cap(this, "userhost-in-names")
	{
	}

	ModResult OnNamesListItem(LocalUser* issuer, Membership* memb, std::string& prefixes, std::string& nick) override
	{
		if (cap.IsEnabled(issuer))
			nick = memb->user->GetMask();

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleUHNames)
