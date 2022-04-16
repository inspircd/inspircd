/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2018 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2017-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014, 2016 Attila Molnar <attilamolnar@hush.com>
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

ListModeBase::ListModeBase(Module* Creator, const std::string& Name, char modechar, unsigned int lnum, unsigned int eolnum, bool autotidy)
	: ModeHandler(Creator, Name, modechar, PARAM_ALWAYS, MODETYPE_CHANNEL, MC_LIST)
	, listnumeric(lnum)
	, endoflistnumeric(eolnum)
	, tidy(autotidy)
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

	user->WriteNumeric(endoflistnumeric, channel->name, InspIRCd::Format("End of channel %s list.", name.c_str()));
}

void ListModeBase::DisplayEmptyList(User* user, Channel* channel)
{
	user->WriteNumeric(endoflistnumeric, channel->name, InspIRCd::Format("Channel %s list is empty.", name.c_str()));
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
		if (!mname.empty() && !stdalgo::string::equalsci(mname, name) && !(mname.length() == 1 && GetModeChar() == mname[0]))
			continue;

		ListLimit limit(c->getString("chan", "*", 1), c->getUInt("limit", DEFAULT_LIST_SIZE));

		if (limit.mask.empty())
			throw ModuleException(creator, InspIRCd::Format("<maxlist:chan> is empty, at %s", c->source.str().c_str()));

		if (limit.mask == "*" || limit.mask == "#*")
			seen_default = true;

		newlimits.push_back(limit);
	}

	// If no default limit has been specified then insert one.
	if (!seen_default)
	{
		ServerInstance->Logs.Log("MODE", LOG_DEBUG, "No default <maxlist> entry was found for the %s mode; defaulting to %u",
			name.c_str(), DEFAULT_LIST_SIZE);
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
			cd->maxitems = -1;
	}
}

unsigned long ListModeBase::FindLimit(const std::string& channame)
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

unsigned long ListModeBase::GetLimitInternal(const std::string& channame, ChanData* cd)
{
	if (cd->maxitems < 0)
		cd->maxitems = FindLimit(channame);
	return cd->maxitems;
}

unsigned long ListModeBase::GetLimit(Channel* channel)
{
	ChanData* cd = extItem.Get(channel);
	if (!cd) // just find the limit
		return FindLimit(channel->name);

	return GetLimitInternal(channel->name, cd);
}

unsigned long ListModeBase::GetLowerLimit()
{
	if (chanlimits.empty())
		return DEFAULT_LIST_SIZE;

	unsigned long limit = UINT_MAX;
	for (const auto& chanlimit : chanlimits)
	{
		if (chanlimit.limit < limit)
			limit = chanlimit.limit;
	}
	return limit;
}

ModeAction ListModeBase::OnModeChange(User* source, User*, Channel* channel, Modes::Change& change)
{
	// Try and grab the list
	ChanData* cd = extItem.Get(channel);

	if (change.adding)
	{
		if (tidy)
			ModeParser::CleanMask(change.param);

		// If there was no list
		if (!cd)
		{
			// Make one
			cd = new ChanData;
			extItem.Set(channel, cd);
		}

		// Check if the item already exists in the list
		for (const auto& entry : cd->list)
		{
			if (change.param == entry.mask)
			{
				/* Give a subclass a chance to error about this */
				TellAlreadyOnList(source, channel, change.param);

				// it does, deny the change
				return MODEACTION_DENY;
			}
		}

		if ((IS_LOCAL(source)) && (cd->list.size() >= GetLimitInternal(channel->name, cd)))
		{
			/* List is full, give subclass a chance to send a custom message */
			TellListTooLong(source, channel, change.param);
			return MODEACTION_DENY;
		}

		/* Ok, it *could* be allowed, now give someone subclassing us
		 * a chance to validate the parameter.
		 * The param is passed by reference, so they can both modify it
		 * and tell us if we allow it or not.
		 *
		 * eg, the subclass could:
		 * 1) allow
		 * 2) 'fix' parameter and then allow
		 * 3) deny
		 */
		if (ValidateParam(source, channel, change.param))
		{
			// And now add the mask onto the list...
			cd->list.emplace_back(change.param, change.set_by.value_or(source->nick), change.set_at.value_or(ServerInstance->Time()));
			return MODEACTION_ALLOW;
		}
		else
		{
			/* If they deny it they have the job of giving an error message */
			return MODEACTION_DENY;
		}
	}
	else
	{
		// We're taking the mode off
		if (cd)
		{
			for (ModeList::iterator it = cd->list.begin(); it != cd->list.end(); ++it)
			{
				if (change.param == it->mask)
				{
					stdalgo::vector::swaperase(cd->list, it);
					return MODEACTION_ALLOW;
				}
			}
		}

		/* Tried to remove something that wasn't set */
		TellNotSet(source, channel, change.param);
		return MODEACTION_DENY;
	}
}

bool ListModeBase::ValidateParam(User* user, Channel* channel, const std::string& parameter)
{
	return true;
}

void ListModeBase::OnParameterMissing(User*, User*, Channel*)
{
	// Intentionally left blank.
}

void ListModeBase::TellListTooLong(User* source, Channel* channel, std::string& parameter)
{
	source->WriteNumeric(ERR_BANLISTFULL, channel->name, parameter, mode, InspIRCd::Format("Channel %s list is full", name.c_str()));
}

void ListModeBase::TellAlreadyOnList(User* source, Channel* channel, std::string& parameter)
{
	source->WriteNumeric(ERR_LISTMODEALREADYSET, channel->name, parameter, mode, InspIRCd::Format("Channel %s list already contains %s", name.c_str(), parameter.c_str()));
}

void ListModeBase::TellNotSet(User* source, Channel* channel, std::string& parameter)
{
	source->WriteNumeric(ERR_LISTMODENOTSET, channel->name, parameter, mode, InspIRCd::Format("Channel %s list does not contain %s", name.c_str(), parameter.c_str()));
}
