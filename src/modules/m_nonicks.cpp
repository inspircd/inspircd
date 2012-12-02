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

/* $ModDesc: Provides support for channel mode +N & extban +b N: which prevents nick changes on channel */

class NoNicks : public SimpleChannelModeHandler
{
 public:
	NoNicks(Module* Creator) : SimpleChannelModeHandler(Creator, "nonick", 'N') { }
};

class ModuleNoNickChange : public Module
{
	NoNicks nn;
	bool override;
 public:
	ModuleNoNickChange() : nn(this)
	{
	}

	void init()
	{
		OnRehash(NULL);
		ServerInstance->Modules->AddService(nn);
		Implementation eventlist[] = { I_OnUserPreNick, I_On005Numeric, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ~ModuleNoNickChange()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides support for channel mode +N & extban +b N: which prevents nick changes on channel", VF_VENDOR);
	}


	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('N');
	}

	virtual ModResult OnUserPreNick(User* user, const std::string &newnick)
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		// Allow forced nick changes.
		if (ServerInstance->NICKForced.get(user))
			return MOD_RES_PASSTHRU;

		for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
		{
			Channel* curr = *i;

			ModResult res = ServerInstance->OnCheckExemption(user,curr,"nonick");

			if (res == MOD_RES_ALLOW)
				continue;

			if (override && IS_OPER(user))
				continue;

			if (!curr->GetExtBanStatus(user, 'N').check(!curr->IsModeSet('N')))
			{
				user->WriteNumeric(ERR_CANTCHANGENICK, "%s :Can't change nickname while on %s (+N is set)",
					user->nick.c_str(), curr->name.c_str());
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	virtual void OnRehash(User* user)
	{
		override = ServerInstance->Config->ConfValue("nonicks")->getBool("operoverride", false);
	}
};

MODULE_INIT(ModuleNoNickChange)
