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
	std::bitset<UCHAR_MAX> lowercase;
	std::bitset<UCHAR_MAX> uppercase;

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
			if (!IS_LOCAL(user))
				return MOD_RES_PASSTHRU;

			Channel* c = (Channel*)dest;
			ModResult res = CheckExemption::Call(exemptionprov, user, c, "blockcaps");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			if (!c->GetExtBanStatus(user, 'B').check(!c->IsModeSet(bc)))
			{
				// If the message is a CTCP then we skip it unless it is
				// an ACTION in which case we strip the prefix and suffix.
				std::string::const_iterator text_begin = text.begin();
				std::string::const_iterator text_end = text.end();
				if (text[0] == '\1')
				{
					// If the CTCP is not an action then skip it.
					if (text.compare(0, 8, "\1ACTION ", 8))
						return MOD_RES_PASSTHRU;

					// Skip the CTCP message characters.
					text_begin += 8;
					if (*text.rbegin() == '\1')
						text_end -= 1;
				}

				// If the message is shorter than the minimum length
				// then we don't need to do anything else.
				size_t length = std::distance(text_begin, text_end);
				if (length < minlen)
					return MOD_RES_PASSTHRU;

				// Count the characters to see how many upper case and
				// ignored (non upper or lower) characters there are.
				size_t upper = 0;
				for (std::string::const_iterator iter = text_begin; iter != text_end; ++iter)
				{
					if (uppercase.test(*iter))
						upper += 1;
					else if (!lowercase.test(*iter))
						length -= 1;
				}

				// Calculate the percentage which is upper case. If the
				// message was entirely symbols then it can't contain
				// any upper case letters.
				if (length > 0 && round((upper * 100) / length) >= percent)
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

		lowercase.reset();
		const std::string lower = tag->getString("lowercase", "abcdefghijklmnopqrstuvwxyz");
		for (std::string::const_iterator iter = lower.begin(); iter != lower.end(); ++iter)
			lowercase.set(*iter);

		uppercase.reset();
		const std::string upper = tag->getString("uppercase", tag->getString("capsmap", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
		for (std::string::const_iterator iter = upper.begin(); iter != upper.end(); ++iter)
			uppercase.set(*iter);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support to block all-CAPS channel messages and notices", VF_VENDOR);
	}
};

MODULE_INIT(ModuleBlockCAPS)
