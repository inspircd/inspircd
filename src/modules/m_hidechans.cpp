/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2022, 2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2006, 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "modules/who.h"
#include "modules/whois.h"

/** Handles user mode +I
 */
class HideChans final
	: public SimpleUserMode
{
public:
	HideChans(Module* Creator)
		: SimpleUserMode(Creator, "hidechans", 'I')
	{
	}
};

class ModuleHideChans final
	: public Module
	, public Who::VisibleEventListener
	, public Whois::LineEventListener
{
private:
	bool affectsopers;
	bool hideservices;
	HideChans hm;

	ModResult ShouldHideChans(LocalUser* source, User* target)
	{
		if (source == target)
			return MOD_RES_PASSTHRU; // User is targeting themself.

		if (!target->IsModeSet(hm))
			return MOD_RES_PASSTHRU; // Mode not set on the target.

		if (hideservices && target->server->IsService())
			return MOD_RES_DENY; // Nobody is allowed to see services not even opers.

		if (!affectsopers && source->HasPrivPermission("users/auspex"))
			return MOD_RES_PASSTHRU; // Opers aren't exempt or the oper doesn't have the right priv.

		return MOD_RES_DENY;
	}

public:
	ModuleHideChans()
		: Module(VF_VENDOR, "Adds user mode I (hidechans) which hides the channels users with it set are in from their /WHOIS response.")
		, Who::VisibleEventListener(this)
		, Whois::LineEventListener(this)
		, hm(this)
	{
	}

	void ReadConfig(ConfigStatus& status) override
	{
		const auto& tag = ServerInstance->Config->ConfValue("hidechans");
		affectsopers = tag->getBool("affectsopers");
		hideservices = tag->getBool("hideservices", true);
	}

	ModResult OnWhoVisible(const Who::Request& request, LocalUser* source, Membership* memb) override
	{
		return ShouldHideChans(source, memb->user);
	}

	ModResult OnWhoisLine(Whois::Context& whois, Numeric::Numeric& numeric) override
	{
		if (numeric.GetNumeric() != RPL_WHOISCHANNELS && numeric.GetNumeric() != RPL_CHANNELSMSG)
			return MOD_RES_PASSTHRU;

		return ShouldHideChans(whois.GetSource(), whois.GetTarget());
	}
};

MODULE_INIT(ModuleHideChans)
