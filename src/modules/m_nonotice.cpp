/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2004, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

/* $ModDesc: Provides channel mode +T to block notices to the channel */

class NoNotice : public SimpleChannelModeHandler
{
 public:
	NoNotice(Module* Creator) : SimpleChannelModeHandler(Creator, "nonotice", 'T') { }
};

class ModuleNoNotice : public Module
{
	NoNotice nt;
 public:

	ModuleNoNotice()
		: nt(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(nt);
		Implementation eventlist[] = { I_OnUserPreNotice, I_On005Numeric };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void On005Numeric(std::string &output)
	{
		ServerInstance->AddExtBanChar('T');
	}

	virtual ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		ModResult res;
		if ((target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;
			if (!c->GetExtBanStatus(user, 'T').check(!c->IsModeSet('T')))
			{
				res = ServerInstance->OnCheckExemption(user,c,"nonotice");
				if (res == MOD_RES_ALLOW)
					return MOD_RES_PASSTHRU;
				else
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, "%s %s :Can't send NOTICE to channel (+T set)",user->nick.c_str(), c->name.c_str());
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	virtual ~ModuleNoNotice()
	{
	}

	virtual Version GetVersion()
	{
		return Version("Provides channel mode +T to block notices to the channel", VF_VENDOR);
	}
};

MODULE_INIT(ModuleNoNotice)
