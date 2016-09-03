/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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
#include "modules/account.h"

class ModuleRegOnlyCreate : public Module
{
	UserModeReference regusermode;

 public:
	ModuleRegOnlyCreate()
		: regusermode(this, "u_registered")
	{
	}

	ModResult OnUserPreJoin(LocalUser* user, Channel* chan, const std::string& cname, std::string& privs, const std::string& keygiven) CXX11_OVERRIDE
	{
		if (chan)
			return MOD_RES_PASSTHRU;

		if (user->IsOper())
			return MOD_RES_PASSTHRU;

		if (user->IsModeSet(regusermode))
			return MOD_RES_PASSTHRU;

		const AccountExtItem* ext = GetAccountExtItem();
		if (ext && ext->get(user))
			return MOD_RES_PASSTHRU;

		// XXX. there may be a better numeric for this..
		user->WriteNumeric(ERR_CHANOPRIVSNEEDED, cname, "You must have a registered nickname to create a new channel");
		return MOD_RES_DENY;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Prevents users whose nicks are not registered from creating new channels", VF_VENDOR);
	}
};

MODULE_INIT(ModuleRegOnlyCreate)
