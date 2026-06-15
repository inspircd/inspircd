/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2017-2023, 2025 Sadie Powell <sadie@sadiepowell.dev>
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
#include "utility/container.h"

ListModeBase::ListModeBase(const WeakModulePtr& Creator, const std::string& Name, char modechar, unsigned int lnum, unsigned int eolnum, bool am)
	: ModeHandler(Creator, Name, modechar, PARAM_ALWAYS, MODETYPE_CHANNEL, MC_LIST)
	, listdata(Creator, "list-mode-" + Name, ExtensionType::CHANNEL)
	, accepts_mask(am)
	, listnumeric(lnum)
	, endoflistnumeric(eolnum)
{
	list = true;
}

void ListModeBase::DisplayList(User* user, Channel* channel)
{
	const auto* data = listdata.Get(channel);
	if (!data || data->list.empty())
	{
		this->DisplayEmptyList(user, channel);
		return;
	}

	for (const auto& item : data->list)
		user->WriteNumeric(listnumeric, channel->name, item.mask, item.setter, item.time);

	user->WriteNumeric(endoflistnumeric, channel->name, FMT::format("End of channel {} list.",
		this->service_name));
}

void ListModeBase::DisplayEmptyList(User* user, Channel* channel)
{
	user->WriteNumeric(endoflistnumeric, channel->name, FMT::format("Channel {} list is empty.",
		this->service_name));
}

void ListModeBase::RemoveMode(Channel* channel, Modes::ChangeList& changelist)
{
	const auto* data = listdata.Get(channel);
	if (data)
	{
		for (const auto& entry : data->list)
			changelist.push_remove(this, entry.mask);
	}
}

size_t ListModeBase::FindLimit(const Channel* chan) const
{
	auto newlimit = ListModeBase::DEFAULT_LIST_SIZE;
	for (const auto& limit : ServerInstance->Config->Limits.MaxList)
	{
		if (!InspIRCd::Match(chan->name, limit.chan))
			continue; // Channel does not match.

		if (!limit.mode.empty() && !this->IsSameMode(limit.mode))
			continue; // Mode name does not match.

		newlimit = limit.limit;
		break;
	}

	ServerInstance->Logs.Debug("MODE", "List mode limit for +{} ({}) on {}: {}",
		this->GetModeChar(), this->service_name, chan->name, newlimit);

	return newlimit;
}

size_t ListModeBase::GetLimit(Channel* chan, ListData* data)
{
	if (!data)
		data = &listdata.GetRef(chan);

	if (data->maxchecked < ServerInstance->Config->ReadTime)
	{
		data->maxchecked = ServerInstance->Time();
		data->maxentries = FindLimit(chan);
	}
	return data->maxentries;
}

bool ListModeBase::OnModeChange(User* source, User*, Channel* channel, Modes::Change& change)
{
	auto* lsource = source->AsLocal();
	if (change.adding)
	{
		// Try to canonicalise the parameter locally.
		if (lsource && !ValidateParam(lsource, channel, change.param))
			return false;

		// Check if the item already exists in the list
		auto& data = listdata.GetRef(channel);
		for (const auto& entry : data.list)
		{
			if (!CompareEntry(entry.mask, change.param))
				continue; // Doesn't match the proposed addition.

			if (lsource)
				TellAlreadyOnList(lsource, channel, change.param);

			return false;
		}

		if (lsource)
		{
			const auto limit = GetLimit(channel, &data);
			if (data.list.size() >= limit)
			{
				// The list size might be 0 so we have to check even if just created.
				TellListTooLong(lsource, channel, change.param, limit);
				return false;
			}
		}

		// Add the new entry to the list.
		data.list.emplace_back(
			change.param,
			change.set_by.value_or(ServerInstance->Config->MaskInList ? source->GetMask() : source->nick),
			change.set_at.value_or(ServerInstance->Time())
		);
		return true;
	}
	else
	{
		auto* data = listdata.Get(channel);
		if (data)
		{
			// We have a list and we're removing; is the entry in it?
			for (auto it = data->list.begin(); it != data->list.end(); ++it)
			{
				if (!CompareEntry(it->mask, change.param))
					continue; // Doesn't match the proposed removal.

				change.param = it->mask;
				insp::swap_erase(data->list, it);
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
	source->WriteNumeric(ERR_BANLISTFULL, channel->name, parameter, mode, FMT::format("Channel {} list is full ({} entries)",
		this->service_name, limit));
}

void ListModeBase::TellAlreadyOnList(LocalUser* source, Channel* channel, const std::string& parameter)
{
	source->WriteNumeric(ERR_LISTMODEALREADYSET, channel->name, parameter, mode, FMT::format("Channel {} list already contains {}",
		this->service_name, parameter));
}

void ListModeBase::TellNotSet(LocalUser* source, Channel* channel, const std::string& parameter)
{
	source->WriteNumeric(ERR_LISTMODENOTSET, channel->name, parameter, mode, FMT::format("Channel {} list does not contain {}",
		this->service_name, parameter));
}
