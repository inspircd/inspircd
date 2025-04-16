/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2015 Renegade334 <contact.caaeed4f@renegade334.me.uk>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@znc.in>
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

class RealMaskExtBan final
	: public ExtBan::MatchingBase
{
public:
	RealMaskExtBan(Module* Creator)
		: ExtBan::MatchingBase(Creator, "realmask", 'a')
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text, bool full) override
	{
		// Check that the user actually specified a real name.
		const size_t divider = text.find('+', 1);
		if (divider == std::string::npos)
			return false;

		// Check whether the user's mask matches.
		if (!channel->CheckBan(user, text.substr(0, divider), full))
			return false;

		// Check whether the user's real name matches.
		return InspIRCd::Match(user->GetRealName(), text.substr(divider + 1));
	}
};

class RealNameExtBan final
	: public ExtBan::MatchingBase
{
public:
	RealNameExtBan(Module* Creator)
		: ExtBan::MatchingBase(Creator, "realname", 'r')
	{
	}

	bool IsMatch(User* user, Channel* channel, const std::string& text, bool full) override
	{
		return InspIRCd::Match(user->GetRealName(), text);
	}
};

class ModuleGecosBan final
	: public Module
{
private:
	RealMaskExtBan maskextban;
	RealNameExtBan realextban;

public:
	ModuleGecosBan()
		: Module(VF_VENDOR | VF_OPTCOMMON, "Adds extended bans a: (realmask) and r:(realname) which checks whether users have a real name matching the specified glob pattern.")
		, maskextban(this)
		, realextban(this)
	{
	}
};

MODULE_INIT(ModuleGecosBan)
