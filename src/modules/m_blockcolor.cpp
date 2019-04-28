/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2004-2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2005, 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2006 Oliver Lupton <oliverlupton@gmail.com>
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

class ModuleBlockColor : public Module
{
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelModeHandler bc;
 public:

	ModuleBlockColor()
		: exemptionprov(this)
		, bc(this, "blockcolor", 'c')
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('c');
	}

	ModResult OnUserPreMessage(User* user, const MessageTarget& target, MessageDetails& details) CXX11_OVERRIDE
	{
		if ((target.type == MessageTarget::TYPE_CHANNEL) && (IS_LOCAL(user)))
		{
			Channel* c = target.Get<Channel>();
			ModResult res = CheckExemption::Call(exemptionprov, user, c, "blockcolor");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			if (!c->GetExtBanStatus(user, 'c').check(!c->IsModeSet(bc)))
			{
				for (std::string::iterator i = details.text.begin(); i != details.text.end(); i++)
				{
					// Block all control codes except \001 for CTCP
					if ((*i >= 0) && (*i < 32) && (*i != 1))
					{
						user->WriteNumeric(ERR_CANNOTSENDTOCHAN, c->name, "Can't send colors to channel (+c is set)");
						return MOD_RES_DENY;
					}
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides channel mode +c to block color",VF_VENDOR);
	}
};

MODULE_INIT(ModuleBlockColor)
