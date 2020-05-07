/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Sadie Powell <sadie@witchery.services>
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
#include "core_channel.h"

void ExtBanManager::AddExtBan(ExtBan::Base* extban)
{
	byletter.emplace(extban->GetLetter(), extban);
	byname.emplace(extban->GetName(), extban);
}

void ExtBanManager::BuildISupport(std::string& out)
{
	for (const LetterMap::value_type& extban : byletter)
		out.push_back(extban.first);

	std::sort(out.begin(), out.end());
	out.insert(0, ",");
}

ModResult ExtBanManager::GetStatus(ExtBan::Acting* extban, User* user, Channel* channel) const
{
	ModResult res;
	FIRST_MOD_RESULT_CUSTOM(evprov, ExtBan::EventListener, OnExtBanCheck, res, (user, channel, extban));
	if (res != MOD_RES_PASSTHRU)
		return res;

	ListModeBase::ModeList* list = banmode.GetList(channel);
	if (!list)
		return MOD_RES_PASSTHRU;

	for (const ListModeBase::ListItem& ban : *list)
	{
		bool inverted;
		std::string xbname, xbvalue;
		if (!ExtBan::Parse(ban.mask, xbname, xbvalue, inverted))
			continue;

		if (xbname.size() == 1)
		{
			// It is an extban but not this extban.
			if (xbname[0] != extban->GetLetter())
				continue;
		}
		else
		{
			// It is an extban but not this extban.
			if (!irc::equals(xbname, extban->GetName()))
				continue;
		}

		return extban->IsMatch(user, channel, xbvalue) != inverted ? MOD_RES_DENY : MOD_RES_PASSTHRU;
	}
	return MOD_RES_PASSTHRU;
}


void ExtBanManager::DelExtBan(ExtBan::Base* extban)
{
	byletter.erase(extban->GetLetter());
	byname.erase(extban->GetName());
}

ExtBan::Base* ExtBanManager::FindName(const std::string& xbname) const
{
	NameMap::const_iterator iter = byname.find(xbname);
	if (iter == byname.end())
		return nullptr;
	return iter->second;
}

ExtBan::Base* ExtBanManager::FindLetter(unsigned char letter) const
{
	LetterMap::const_iterator iter = byletter.find(letter);
	if (iter == byletter.end())
		return nullptr;
	return iter->second;
}
