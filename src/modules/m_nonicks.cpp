/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2004, 2006 Craig Edwards <craigedwards@brainbox.cc>
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

class ModuleNoNickChange : public Module
{
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelModeHandler nn;
 public:
	ModuleNoNickChange()
		: exemptionprov(this)
		, nn(this, "nonick", 'N')
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +N and extban 'N' which prevents nick changes on the channel", VF_VENDOR);
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('N');
	}

	ModResult OnUserPreNick(LocalUser* user, const std::string& newnick) CXX11_OVERRIDE
	{
		for (User::ChanList::iterator i = user->chans.begin(); i != user->chans.end(); i++)
		{
			Channel* curr = (*i)->chan;

			ModResult res = CheckExemption::Call(exemptionprov, user, curr, "nonick");

			if (res == MOD_RES_ALLOW)
				continue;

			if (user->HasPrivPermission("channels/ignore-nonicks"))
				continue;

			if (!curr->GetExtBanStatus(user, 'N').check(!curr->IsModeSet(nn)))
			{
				user->WriteNumeric(ERR_CANTCHANGENICK, InspIRCd::Format("Cannot change nickname while on %s (+N is set)",
					curr->name.c_str()));
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleNoNickChange)
