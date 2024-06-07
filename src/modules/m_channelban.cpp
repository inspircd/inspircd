/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
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
#include "modules/extban.h"

class ChannelExtBan final
	: public ExtBan::MatchingBase
{
public:
	ChannelExtBan(Module* Creator)
		: ExtBan::MatchingBase(Creator, "channel", 'j')
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text) override
	{
		unsigned char status = 0;
		const char* target = text.c_str();
		const PrefixMode* const mh = ServerInstance->Modes.FindPrefix(text[0]);
		if (mh)
		{
			status = mh->GetModeChar();
			target++;
		}
		for (auto* memb : user->chans)
		{
			if (!InspIRCd::Match(memb->chan->name, target))
				continue;
			if (!status || memb->GetRank() >= mh->GetPrefixRank())
				return true;
		}
		return false;
	}
};

class ModuleBadChannelExtban final
	: public Module
{
private:
	ChannelExtBan extban;

public:
	ModuleBadChannelExtban()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds extended ban j: (channel) which checks whether users are in a channel matching the specified glob pattern.")
		, extban(this)
	{
	}
};

MODULE_INIT(ModuleBadChannelExtban)
