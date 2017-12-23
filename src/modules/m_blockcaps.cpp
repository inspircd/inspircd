/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2006, 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2006-2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

class ModuleBlockCAPS : public Module
{
	CheckExemption::EventProvider exemptionprov;
	SimpleChannelModeHandler bc;
	unsigned int percent;
	unsigned int minlen;
	std::bitset<UCHAR_MAX> capsmap;

public:
	ModuleBlockCAPS()
		: exemptionprov(this)
		, bc(this, "blockcaps", 'B')
	{
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		tokens["EXTBAN"].push_back('B');
	}

	ModResult OnUserPreMessage(User* user, void* dest, int target_type, std::string& text, char status, CUList& exempt_list, MessageType msgtype) CXX11_OVERRIDE
	{
		if (target_type == TYPE_CHANNEL)
		{
			if ((!IS_LOCAL(user)) || (text.length() < minlen) || (text == "\1ACTION\1") || (text == "\1ACTION"))
				return MOD_RES_PASSTHRU;

			Channel* c = (Channel*)dest;
			ModResult res = CheckExemption::Call(exemptionprov, user, c, "blockcaps");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			if (!c->GetExtBanStatus(user, 'B').check(!c->IsModeSet(bc)))
			{
				std::string::size_type caps = 0;
				unsigned int offset = 0;
				// Ignore the beginning of the text if it's a CTCP ACTION (/me)
				if (!text.compare(0, 8, "\1ACTION ", 8))
					offset = 8;

				for (std::string::const_iterator i = text.begin() + offset; i != text.end(); ++i)
					if (capsmap.test(*i))
						caps += 1;

				if (((caps * 100) / text.length()) >= percent)
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, c->name, InspIRCd::Format("Your message cannot contain %d%% or more capital letters if it's longer than %d characters", percent, minlen));
					return MOD_RES_DENY;
				}
			}
		}
		return MOD_RES_PASSTHRU;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("blockcaps");
		percent = tag->getInt("percent", 100, 1, 100);
		minlen = tag->getInt("minlen", 1, 1, ServerInstance->Config->Limits.MaxLine);
		std::string hmap = tag->getString("capsmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		capsmap.reset();
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			capsmap.set(*n);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support to block all-CAPS channel messages and notices", VF_VENDOR);
	}
};

MODULE_INIT(ModuleBlockCAPS)
