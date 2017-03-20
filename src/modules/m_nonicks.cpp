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

class NoNicks : public SimpleChannelModeHandler
{
 public:
	NoNicks(Module* Creator) : SimpleChannelModeHandler(Creator, "nonick", 'N') { }
};

class ModuleNoNickChange : public Module
{
	CheckExemption::EventProvider exemptionprov;
	NoNicks nn;
	bool override;
 public:
	ModuleNoNickChange()
		: exemptionprov(this)
		, nn(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support for channel mode +N & extban +b N: which prevents nick changes on channel", VF_VENDOR);
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

			ModResult res;
			FIRST_MOD_RESULT_CUSTOM(exemptionprov, CheckExemption::EventListener, OnCheckExemption, res, (user, curr, "nonick"));

			if (res == MOD_RES_ALLOW)
				continue;

			if (override && user->IsOper())
				continue;

			if (!curr->GetExtBanStatus(user, 'N').check(!curr->IsModeSet(nn)))
			{
				user->WriteNumeric(ERR_CANTCHANGENICK, InspIRCd::Format("Can't change nickname while on %s (+N is set)",
					curr->name.c_str()));
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		override = ServerInstance->Config->ConfValue("nonicks")->getBool("operoverride", false);
	}
};

MODULE_INIT(ModuleNoNickChange)
