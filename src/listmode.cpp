/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2017-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
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
#include "listmode.h"

ListModeBase::ListModeBase(Module* Creator, const std::string& Name, char modechar, unsigned int lnum, unsigned int eolnum)
	: ModeHandler(Creator, Name, modechar, PARAM_ALWAYS, MODETYPE_CHANNEL, MC_LIST)
	, listnumeric(lnum)
	, endoflistnumeric(eolnum)
	, extItem(Creator, "list-mode-" + name, ExtensionType::CHANNEL)
{
	list = true;
}

void ListModeBase::DisplayList(User* user, Channel* channel)
{
	ChanData* cd = extItem.Get(channel);
	if (!cd || cd->list.empty())
	{
		this->DisplayEmptyList(user, channel);
		return;
	}

	for (const auto& item : cd->list)
		user->WriteNumeric(listnumeric, channel->name, item.mask, item.setter, item.time);

	user->WriteNumeric(endoflistnumeric, channel->name, INSP_FORMAT("End of channel {} list.", name));
}

void ListModeBase::DisplayEmptyList(User* user, Channel* channel)
{
	user->WriteNumeric(endoflistnumeric, channel->name, INSP_FORMAT("Channel {} list is empty.", name));
}

void ListModeBase::RemoveMode(Channel* channel, Modes::ChangeList& changelist)
{
	ChanData* cd = extItem.Get(channel);
	if (cd)
	{
		for (const auto& entry : cd->list)
			changelist.push_remove(this, entry.mask);
	}
}

void ListModeBase::DoRehash()
{
	limitlist newlimits;
	bool seen_default = false;

	for (const auto& [_, c] : ServerInstance->Config->ConfTags("maxlist"))
	{
		const std::string mname = c->getString("mode");
		if (!mname.empty() && !insp::equalsci(mname, name) && !(mname.length() == 1 && GetModeChar() == mname[0]))
			continue;

		ListLimit limit(c->getString("chan", "*", 1), c->getNum<size_t>("limit", DEFAULT_LIST_SIZE));

		if (limit.mask.empty())
			throw ModuleException(creator, INSP_FORMAT("<maxlist:chan> is empty, at {}", c->source.str()));

		if (limit.mask == "*" || limit.mask == "#*")
			seen_default = true;

		newlimits.push_back(limit);
	}

	// If no default limit has been specified then insert one.
	if (!seen_default)
	{
		ServerInstance->Logs.Warning("MODE", "No default <maxlist> entry was found for the {} mode; defaulting to {}",
			name, DEFAULT_LIST_SIZE);
		newlimits.push_back(ListLimit("*", DEFAULT_LIST_SIZE));
	}

	// Most of the time our settings are unchanged, so we can avoid iterating the chanlist
	if (chanlimits == newlimits)
		return;

	chanlimits.swap(newlimits);

	for (const auto& [_, chan] : ServerInstance->Channels.GetChans())
	{
		ChanData* cd = extItem.Get(chan);
		if (cd)
			cd->maxitems.reset();
	}
}

size_t ListModeBase::FindLimit(const std::string& channame)
{
	for (const auto& chanlimit : chanlimits)
	{
		if (InspIRCd::Match(channame, chanlimit.mask))
		{
			// We have a pattern matching the channel
			return chanlimit.limit;
		}
	}
	return 0;
}

size_t ListModeBase::GetLimitInternal(const std::string& channame, ChanData* cd)
{
	if (!cd->maxitems)
		cd->maxitems = FindLimit(channame);
	return cd->maxitems.value();
}

size_t ListModeBase::GetLimit(Channel* channel)
{
	ChanData* cd = extItem.Get(channel);
	if (!cd) // just find the limit
		return FindLimit(channel->name);

	return GetLimitInternal(channel->name, cd);
}

size_t ListModeBase::GetLowerLimit()
{
	if (chanlimits.empty())
		return DEFAULT_LIST_SIZE;

	size_t limit = std::numeric_limits<size_t>::max();
	for (const auto& chanlimit : chanlimits)
	{
		if (chanlimit.limit < limit)
			limit = chanlimit.limit;
	}
	return limit;
}

bool ListModeBase::OnModeChange(User* source, User*, Channel* channel, Modes::Change& change)
{
	LocalUser* lsource = IS_LOCAL(source);
	if (change.adding)
	{
		// Try to canonicalise the parameter locally.
		if (lsource && !ValidateParam(lsource, channel, change.param))
			return false;

		ChanData* cd = extItem.Get(channel);
		if (cd)
		{
			// Check if the item already exists in the list
			for (const auto& entry : cd->list)
			{
				if (!CompareEntry(entry.mask, change.param))
					continue; // Doesn't match the proposed addition.

				if (lsource)
					TellAlreadyOnList(lsource, channel, change.param);

				return false;
			}
		}
		else
		{
			// There's no channel list currently; create one.
			cd = new ChanData;
			extItem.Set(channel, cd);
		}

		if (lsource)
		{
			size_t limit = GetLimitInternal(channel->name, cd);
			if (cd->list.size() >= limit)
			{
				// The list size might be 0 so we have to check even if just created.
				TellListTooLong(lsource, channel, change.param, limit);
				return false;
			}
		}

		// Add the new entry to the list.
		cd->list.emplace_back(
			change.param,
			change.set_by.value_or(ServerInstance->Config->MaskInList ? source->GetMask() : source->nick),
			change.set_at.value_or(ServerInstance->Time())
		);
		return true;
	}
	else
	{
		ChanData* cd = extItem.Get(channel);
		if (cd)
		{
			// We have a list and we're removing; is the entry in it?
			for (ModeList::iterator it = cd->list.begin(); it != cd->list.end(); ++it)
			{
				if (!CompareEntry(it->mask, change.param))
					continue; // Doesn't match the proposed removal.

				change.param = it->mask;
				stdalgo::vector::swaperase(cd->list, it);
				return true;
			}
		}

		if (lsource)
			TellNotSet(lsource, channel, change.param);

		return false;
	}
}

bool ListModeBase::ValidateParam(LocalUser* user, Channel* channel, std::string& parameter)
{
	return true;
}

void ListModeBase::OnParameterMissing(User* source, User* dest, Channel* channel)
{
	// Intentionally left blank.
}

void ListModeBase::TellListTooLong(LocalUser* source, Channel* channel, const std::string& parameter, size_t limit)
{
	source->WriteNumeric(ERR_BANLISTFULL, channel->name, parameter, mode, INSP_FORMAT("Channel {} list is full ({} entries)", name, limit));
}

void ListModeBase::TellAlreadyOnList(LocalUser* source, Channel* channel, const std::string& parameter)
{
	source->WriteNumeric(ERR_LISTMODEALREADYSET, channel->name, parameter, mode, INSP_FORMAT("Channel {} list already contains {}", name, parameter));
}

void ListModeBase::TellNotSet(LocalUser* source, Channel* channel, const std::string& parameter)
{
	source->WriteNumeric(ERR_LISTMODENOTSET, channel->name, parameter, mode, INSP_FORMAT("Channel {} list does not contain {}", name, parameter));
}
