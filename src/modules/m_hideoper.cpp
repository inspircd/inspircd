/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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

/* $ModDesc: Provides support for hiding oper status with user mode +H */

/** Handles user mode +H
 */
class HideOper : public SimpleUserModeHandler
{
 public:
	HideOper(Module* Creator) : SimpleUserModeHandler(Creator, "hideoper", 'H')
	{
		oper = true;
	}
};

class ModuleHideOper : public Module
{
	HideOper hm;
 public:
	ModuleHideOper()
		: hm(this)
	{
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->Modules->AddService(hm);
		Implementation eventlist[] = { I_OnWhoisLine, I_OnSendWhoLine };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for hiding oper status with user mode +H", VF_VENDOR);
	}

	ModResult OnWhoisLine(User* user, User* dest, int &numeric, std::string &text) CXX11_OVERRIDE
	{
		/* Dont display numeric 313 (RPL_WHOISOPER) if they have +H set and the
		 * person doing the WHOIS is not an oper
		 */
		if (numeric != 313)
			return MOD_RES_PASSTHRU;

		if (!dest->IsModeSet('H'))
			return MOD_RES_PASSTHRU;

		if (!user->HasPrivPermission("users/auspex"))
			return MOD_RES_DENY;

		return MOD_RES_PASSTHRU;
	}

	void OnSendWhoLine(User* source, const std::vector<std::string>& params, User* user, std::string& line) CXX11_OVERRIDE
	{
		if (user->IsModeSet('H') && !source->HasPrivPermission("users/auspex"))
		{
			// hide the "*" that marks the user as an oper from the /WHO line
			std::string::size_type pos = line.find("*");
			if (pos != std::string::npos)
				line.erase(pos, 1);
			// hide the line completely if doing a "/who * o" query
			if (params.size() > 1 && params[1].find('o') != std::string::npos)
				line.clear();
		}
	}
};

MODULE_INIT(ModuleHideOper)
