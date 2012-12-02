/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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

/* $ModDesc: Forces users to join the specified channel(s) on connect */

class ModuleConnJoin : public Module
{
	public:
		void init()
		{
			Implementation eventlist[] = { I_OnPostConnect };
			ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		}

		void Prioritize()
		{
			ServerInstance->Modules->SetPriority(this, I_OnPostConnect, PRIORITY_LAST);
		}

		Version GetVersion()
		{
			return Version("Forces users to join the specified channel(s) on connect", VF_VENDOR);
		}

		void OnPostConnect(User* user)
		{
			if (!IS_LOCAL(user))
				return;

			std::string chanlist = ServerInstance->Config->ConfValue("autojoin")->getString("channel");
			chanlist = user->GetClass()->config->getString("autojoin", chanlist);

			irc::commasepstream chans(chanlist);
			std::string chan;

			while (chans.GetToken(chan))
			{
				if (ServerInstance->IsChannel(chan.c_str(), ServerInstance->Config->Limits.ChanMax))
					Channel::JoinUser(user, chan.c_str(), false, "", false, ServerInstance->Time());
			}
		}
};


MODULE_INIT(ModuleConnJoin)
