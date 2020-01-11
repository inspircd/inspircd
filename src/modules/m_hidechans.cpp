/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2013, 2015-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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
#include "modules/whois.h"

/** Handles user mode +I
 */
class HideChans : public SimpleUserModeHandler
{
 public:
	HideChans(Module* Creator) : SimpleUserModeHandler(Creator, "hidechans", 'I') { }
};

class ModuleHideChans : public Module, public Whois::LineEventListener
{
	bool AffectsOpers;
	HideChans hm;
 public:
	ModuleHideChans()
		: Whois::LineEventListener(this)
		, hm(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for hiding channels with user mode +I", VF_VENDOR);
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		AffectsOpers = ServerInstance->Config->ConfValue("hidechans")->getBool("affectsopers");
	}

	ModResult OnWhoisLine(Whois::Context& whois, Numeric::Numeric& numeric) CXX11_OVERRIDE
	{
		/* always show to self */
		if (whois.IsSelfWhois())
			return MOD_RES_PASSTHRU;

		/* don't touch anything except 319 */
		if (numeric.GetNumeric() != 319)
			return MOD_RES_PASSTHRU;

		/* don't touch if -I */
		if (!whois.GetTarget()->IsModeSet(hm))
			return MOD_RES_PASSTHRU;

		/* if it affects opers, we don't care if they are opered */
		if (AffectsOpers)
			return MOD_RES_DENY;

		/* doesn't affect opers, sender is opered */
		if (whois.GetSource()->HasPrivPermission("users/auspex"))
			return MOD_RES_PASSTHRU;

		/* user must be opered, boned. */
		return MOD_RES_DENY;
	}
};

MODULE_INIT(ModuleHideChans)
