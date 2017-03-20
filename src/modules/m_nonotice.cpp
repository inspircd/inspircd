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
#include "modules/exemption.h"

class NoNotice : public SimpleChannelModeHandler
{
 public:
	NoNotice(Module* Creator) : SimpleChannelModeHandler(Creator, "nonotice", 'T') { }
};

class ModuleNoNotice : public Module
{
	CheckExemption::EventProvider exemptionprov;
	NoNotice nt;
 public:

	ModuleNoNotice()
		: exemptionprov(this)
		, nt(this)
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('T');
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		ModResult res;
		if ((msgtype == MSG_NOTICE) && (target_type == TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = (Channel*)dest;
			if (!c->GetExtBanStatus(user, 'T').check(!c->IsModeSet(nt)))
			{
				FIRST_MOD_RESULT_CUSTOM(exemptionprov, CheckExemption::EventListener, OnCheckExemption, res, (user, c, "nonotice"));
				if (res == MOD_RES_ALLOW)
					return MOD_RES_PASSTHRU;
				else
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, c->name, "Can't send NOTICE to channel (+T set)");
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +T to block notices to the channel", VF_VENDOR);
	}
};

MODULE_INIT(ModuleNoNotice)
