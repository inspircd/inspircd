/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Shawn Smith <shawn@inspircd.org>
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

/* $ModDesc: Provides support to block all-CAPS channel messages and notices */


class blockcapssettings
{
 public:
	unsigned int percent;
	unsigned int minlen;

	blockcapssettings(int a, int b) : percent(a), minlen(b) { }
};


/** Handles the +B channel mode
 */
class BlockCaps : public ModeHandler
{
 public:
	SimpleExtItem<blockcapssettings> ext;
	BlockCaps(Module* Creator) : ModeHandler(Creator, "blockcaps", 'B', PARAM_SETONLY, MODETYPE_CHANNEL),
		ext("blockcaps", Creator) { }


	/* "Borrowed" most of this logic from m_messageflood, Thanks. -Shawn */
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		if (adding)
		{
			std::string::size_type colon = parameter.find(':');
			if ((colon == std::string::npos) || (parameter.find('-') != std::string::npos))
			{
				source->WriteNumeric(608, "%s %s :Invalid block-caps parameters", source->nick.c_str(), channel->name.c_str());
				return MODEACTION_DENY;
			}

			unsigned int percent = ConvToInt(parameter.substr(0, colon));
			unsigned int minlen = ConvToInt(parameter.substr(colon+1));

			/* percent must be between 1 and 100, minlen must be greater than 1 and less than MAXBUF-1 */
			if (percent <= 0 || percent > 100 || minlen < 1 || minlen > MAXBUF-1)
			{
				source->WriteNumeric(608, "%s %s :Invalid block-caps parameters", source->nick.c_str(), channel->name.c_str());
				return MODEACTION_DENY;
			}

			blockcapssettings* blockcaps = ext.get(channel);

			/* Make sure the settings don't match */
			if ((blockcaps) && (percent == blockcaps->percent) && (minlen == blockcaps->percent))
				return MODEACTION_DENY;

			ext.set(channel, new blockcapssettings(percent, minlen));
			parameter = std::string("" + ConvToStr(percent) + ":" + ConvToStr(minlen));
			channel->SetModeParam('B', parameter);
			return MODEACTION_ALLOW;
		}
		else
		{
			if (!channel->IsModeSet('B'))
				return MODEACTION_DENY;

			ext.unset(channel);
			channel->SetModeParam('B', "");
			return MODEACTION_ALLOW;
		}
	}
};

class ModuleBlockCAPS : public Module
{
	BlockCaps bc;
	char capsmap[256];

public:
	ModuleBlockCAPS() : bc(this) { }

	void init() CXX11_OVERRIDE
	{
		OnRehash(NULL);
		ServerInstance->Modules->AddService(bc);
		Implementation eventlist[] = { I_OnUserPreMessage, I_OnUserPreNotice, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual void OnRehash(User* user)
	{
		ReadConf();
	}

	ModResult ProcessMessages(User* user, Channel* channel, const std::string& text)
	{
		if ((!IS_LOCAL(user) || !channel->IsModeSet('B')))
			return MOD_RES_PASSTHRU;

		if (ServerInstance->OnCheckExemption(user, channel, "blockcaps") == MOD_RES_ALLOW)
			return MOD_RES_PASSTHRU;

		blockcapssettings *blockcaps = bc.ext.get(channel);

		if (text.length() < blockcaps->minlen)
			return MOD_RES_PASSTHRU;

		if (blockcaps)
		{
			unsigned int caps = 0;
			int act = 0;
			const char* actstr = "\1ACTION ";

			for (std::string::const_iterator i = text.begin(); i != text.end(); i++)
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

			if (((caps*100)/(int)text.length()) >= blockcaps->percent)
			{
				user->WriteNumeric(ERR_CANNOTSENDTOCHAN, "%s %s :Your Message cannot contain more than %d%% capital letters if it's longer than %d characters", user->nick.c_str(), channel->name.c_str(), blockcaps->percent, blockcaps->minlen);
				return MOD_RES_DENY;
			}
		}

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreMessage(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
			return ProcessMessages(user, (Channel*)dest, text);

		return MOD_RES_PASSTHRU;
	}

	ModResult OnUserPreNotice(User* user,void* dest,int target_type, std::string &text, char status, CUList &exempt_list)
	{
		if (target_type == TYPE_CHANNEL)
			return ProcessMessages(user,(Channel*)dest, text);

		return MOD_RES_PASSTHRU;
	}

	void ReadConf()
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("blockcaps");
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
