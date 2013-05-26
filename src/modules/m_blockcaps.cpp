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


/** Handles the +B channel mode
 */
class BlockCaps : public SimpleChannelModeHandler
{
 public:
	BlockCaps(Module* Creator) : SimpleChannelModeHandler(Creator, "blockcaps", 'B') { }
};

class ModuleBlockCAPS : public Module
{
	BlockCaps bc;
	int percent;
	unsigned int minlen;
	char capsmap[256];

public:
	ModuleBlockCAPS() : bc(this)
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
			if ((!IS_LOCAL(user)) || (text.length() < minlen))
				return MOD_RES_PASSTHRU;

			Channel* c = (Channel*)dest;
			ModResult res = ServerInstance->OnCheckExemption(user,c,"blockcaps");

			if (res == MOD_RES_ALLOW)
				return MOD_RES_PASSTHRU;

			if (!c->GetExtBanStatus(user, 'B').check(!c->IsModeSet(bc)))
			{
				int caps = 0;
				const char* actstr = "\1ACTION ";
				int act = 0;

				for (std::string::iterator i = text.begin(); i != text.end(); i++)
				{
					/* Smart fix for suggestion from Jobe, ignore CTCP ACTION (part of /ME) */
					if (*actstr && *i == *actstr++ && act != -1)
					{
						act++;
						continue;
					}
					else
						act = -1;

					caps += capsmap[(unsigned char)*i];
				}
				if ( ((caps*100)/(int)text.length()) >= percent )
				{
					user->WriteNumeric(ERR_CANNOTSENDTOCHAN, "%s %s :Your message cannot contain more than %d%% capital letters if it's longer than %d characters", user->nick.c_str(), c->name.c_str(), percent, minlen);
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
		memset(capsmap, 0, sizeof(capsmap));
		for (std::string::iterator n = hmap.begin(); n != hmap.end(); n++)
			capsmap[(unsigned char)*n] = 1;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides support to block all-CAPS channel messages and notices", VF_VENDOR);
	}
};

MODULE_INIT(ModuleBlockCAPS)
