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

class ModuleNoNotice : public Module
{
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelModeHandler nt;
 public:

	ModuleNoNotice()
		: exemptionprov(this)
		, nt(this, "nonotice", 'T')
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('T');
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		ModResult res;
		if ((details.type == MSG_NOTICE) && (target.type == MessageTarget::TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = target.Get<Channel>();
			if (!c->GetExtBanStatus(user, 'T').check(!c->IsModeSet(nt)))
			{
				res = CheckExemption::Call(exemptionprov, user, c, "nonotice");
				if (res == MOD_RES_ALLOW)
					return MOD_RES_PASSTHRU;
				else
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, c->name, "Can't send NOTICE to channel (+T is set)");
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
