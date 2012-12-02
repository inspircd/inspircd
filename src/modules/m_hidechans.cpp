/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

/* $ModDesc: Provides support for hiding channels with user mode +I */

/** Handles user mode +I
 */
class HideChans : public SimpleUserModeHandler
{
 public:
	HideChans(Module* Creator) : SimpleUserModeHandler(Creator, "hidechans", 'I') { }
};

class ModuleHideChans : public Module
{
	bool AffectsOpers;
	HideChans hm;
 public:
	ModuleHideChans() : hm(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(hm);
		Implementation eventlist[] = { I_OnWhoisLine, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	virtual ~ModuleHideChans()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for hiding channels with user mode +I", VF_VENDOR);
	}

	virtual void OnRehash(User* user)
	{
		AffectsOpers = ServerInstance->Config->ConfValue("hidechans")->getBool("affectsopers");
	}

	ModResult OnWhoisLine(User* user, User* dest, int &numeric, std::string &text)
	{
		/* always show to self */
		if (user == dest)
			return MOD_RES_PASSTHRU;

		/* don't touch anything except 319 */
		if (numeric != 319)
			return MOD_RES_PASSTHRU;

		/* don't touch if -I */
		if (!dest->IsModeSet('I'))
			return MOD_RES_PASSTHRU;

		/* if it affects opers, we don't care if they are opered */
		if (AffectsOpers)
			return MOD_RES_DENY;

		/* doesn't affect opers, sender is opered */
		if (user->HasPrivPermission("users/auspex"))
			return MOD_RES_PASSTHRU;

		/* user must be opered, boned. */
		return MOD_RES_DENY;
	}
};


MODULE_INIT(ModuleHideChans)
