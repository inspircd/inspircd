/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Implements extban +b j: - matching channel bans */

class ModuleBadChannelExtban : public Module
{
 private:
 public:
	void init()
	{
		Implementation eventlist[] = { I_OnCheckBan, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	~ModuleBadChannelExtban()
	{
	}

	Version GetVersion()
	{
		return Version("Extban 'j' - channel status/join ban", VF_OPTCOMMON|VF_VENDOR);
	}

	ModResult OnCheckBan(User *user, Channel *c, const std::string& mask)
	{
		if ((mask.length() > 2) && (mask[0] == 'j') && (mask[1] == ':'))
		{
			std::string rm = mask.substr(2);
			char status = 0;
			ModeHandler* mh = ServerInstance->Modes->FindPrefix(rm[0]);
			if (mh)
			{
				rm = mask.substr(3);
				status = mh->GetModeChar();
			}
			for (UCListIter i = user->chans.begin(); i != user->chans.end(); i++)
			{
				if (InspIRCd::Match((**i).name, rm))
				{
					if (status)
					{
						Membership* memb = (**i).GetUser(user);
						if (memb && memb->hasMode(status))
							return MOD_RES_DENY;
					}
					else
						return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('j');
	}
};


MODULE_INIT(ModuleBadChannelExtban)

