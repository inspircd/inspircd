/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020, 2022-2024 Sadie Powell <sadie@witchery.services>
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
	if (extban->GetLetter())
	{
		auto lit = byletter.emplace(extban->GetLetter(), extban);
		if (!lit.second)
			throw ModuleException(creator, INSP_FORMAT("ExtBan letter \"{}\" is already in use by the {} extban from {}",
				extban->GetLetter(), lit.first->second->GetName(), lit.first->second->creator->ModuleFile));
	}

	auto nit = byname.emplace(extban->GetName(), extban);
	if (!nit.second)
	{
		if (extban->GetLetter())
			byletter.erase(extban->GetLetter());

		throw ModuleException(creator, INSP_FORMAT("ExtBan name \"{}\" is already in use by the {} extban from {}",
			extban->GetName(), nit.first->second->GetLetter(), nit.first->second->creator->ModuleFile));
	}
}

bool ExtBanManager::Canonicalize(std::string& text) const
{
	bool inverted; // Intentionally unused
	std::string xbname;
	std::string xbvalue;
	if (!ExtBan::Parse(text, xbname, xbvalue, inverted))
		return false; // Not an extban.

	ExtBan::Base* extban = nullptr;
	if (xbname.size() == 1)
		extban = FindLetter(xbname[0]);
	else
		extban = FindName(xbname);

	if (!extban)
		return false; // Looks like an extban but it isn't.

	// Canonicalize the extban.
	text.assign(inverted ? "!" : "");

	switch (format)
	{
		case ExtBan::Format::NAME:
			text.append(extban->GetName());
			break;

		case ExtBan::Format::LETTER:
			if (extban->GetLetter())
				text.push_back(extban->GetLetter());
			else
				text.append(extban->GetName()); // ExtBan has no letter.
			break;

		default:
			text.append(xbname);
			break;
	}

	extban->Canonicalize(xbvalue);
	text.append(":").append(xbvalue);

	return true;
}

ExtBan::Comparison ExtBanManager::CompareEntry(const ListModeBase* lm, const std::string& entry, const std::string& value) const
{
	bool entry_inverted;
	std::string entry_xbname;
	std::string entry_xbvalue;
	bool entry_result = ExtBan::Parse(entry, entry_xbname, entry_xbvalue, entry_inverted);

	bool value_inverted;
	std::string value_xbname;
	std::string value_xbvalue;
	bool value_result = ExtBan::Parse(value, value_xbname, value_xbvalue, value_inverted);

	if (!entry_result && !value_result)
		return ExtBan::Comparison::NOT_AN_EXTBAN;

	// If we've reached this point at least one looks like an extban.
	auto entry_extban = entry_result ? Find(entry_xbname) : nullptr;
	auto value_extban = value_result ? Find(value_xbname) : nullptr;
	if (!entry_extban || !value_extban)
		return ExtBan::Comparison::NOT_MATCH;

	// If we've reached this point both are extbans so we can just do a simple comparison.
	if (entry_inverted != value_inverted || entry_extban != value_extban)
		return ExtBan::Comparison::NOT_MATCH;

	// Compare the values recursively.
	return lm->CompareEntry(entry_xbvalue, value_xbvalue) ? ExtBan::Comparison::MATCH : ExtBan::Comparison::NOT_MATCH;
}

void ExtBanManager::BuildISupport(std::string& out)
{
	for (const auto& extban : byletter)
		out.push_back(extban.first);

	std::sort(out.begin(), out.end());
	out.insert(0, ",");
}

ModResult ExtBanManager::GetStatus(ExtBan::ActingBase* extban, User* user, Channel* channel) const
{
	ModResult res = evprov.FirstResult(&ExtBan::EventListener::OnExtBanCheck, user, channel, extban);
	if (res != MOD_RES_PASSTHRU)
		return res;

	ListModeBase::ModeList* list = banmode.GetList(channel);
	if (!list)
		return MOD_RES_PASSTHRU;

	for (const auto& ban : *list)
	{
		bool inverted;
		std::string xbname;
		std::string xbvalue;
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

		// For a non-inverted (regular) extban we want to match but for an
		// inverted extban we want to not match.
		if (extban->IsMatch(user, channel, xbvalue) != inverted)
			return MOD_RES_DENY;
	}
	return MOD_RES_PASSTHRU;
}

void ExtBanManager::DelExtBan(ExtBan::Base* extban)
{
	if (extban->GetLetter())
	{
		auto lit = byletter.find(extban->GetLetter());
		if (lit != byletter.end() && lit->second->creator.ptr() == extban->creator.ptr())
			byletter.erase(lit);
	}

	auto nit = byname.find(extban->GetName());
	if (nit != byname.end() && nit->second->creator.ptr() == extban->creator.ptr())
		byname.erase(nit);
}

ExtBan::Base* ExtBanManager::FindName(const std::string& xbname) const
{
	NameMap::const_iterator iter = byname.find(xbname);
	if (iter == byname.end())
		return nullptr;
	return iter->second;
}

ExtBan::Base* ExtBanManager::FindLetter(ExtBan::Letter letter) const
{
	LetterMap::const_iterator iter = byletter.find(letter);
	if (iter == byletter.end())
		return nullptr;
	return iter->second;
}
