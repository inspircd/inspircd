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

class ModuleStripColor : public Module
{
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelModeHandler csc;
	SimpleUserModeHandler usc;

 public:
	ModuleStripColor()
		: exemptionprov(this)
		, csc(this, "stripcolor", 'S')
		, usc(this, "u_stripcolor", 'S')
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('S');
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if (!IS_LOCAL(user))
			return MOD_RES_PASSTHRU;

		bool active = false;
		switch (target.type)
		{
			case MessageTarget::TYPE_USER:
			{
				User* t = target.Get<User>();
				active = t->IsModeSet(usc);
				break;
			}
			case MessageTarget::TYPE_CHANNEL:
			{
				Channel* t = target.Get<Channel>();
				ModResult res = CheckExemption::Call(exemptionprov, user, t, "stripcolor");

				if (res == MOD_RES_ALLOW)
					return MOD_RES_PASSTHRU;

				active = !t->GetExtBanStatus(user, 'S').check(!t->IsModeSet(csc));
				break;
			}
			case MessageTarget::TYPE_SERVER:
				break;
		}

		if (active)
		{
			InspIRCd::StripColor(details.text);
		}

		return MOD_RES_PASSTHRU;
	}

	void OnUserPart(Membership* memb, std::string& partmessage, CUList& except_list) CXX11_OVERRIDE
	{
		User* user = memb->user;
		Channel* channel = memb->chan;

		if (!IS_LOCAL(user))
			return;

		if (channel->GetExtBanStatus(user, 'S').check(!user->IsModeSet(csc)))
		{
			ModResult res = CheckExemption::Call(exemptionprov, user, channel, "stripcolor");

			if (res != MOD_RES_ALLOW)
				InspIRCd::StripColor(partmessage);
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +S, strip ansi color", VF_VENDOR);
	}

};

MODULE_INIT(ModuleStripColor)
