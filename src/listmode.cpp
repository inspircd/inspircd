/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

ListModeBase::ListModeBase(Module* Creator, const std::string& Name, char modechar, const std::string &eolstr, unsigned int lnum, unsigned int eolnum, bool autotidy, const std::string &ctag)
	: ModeHandler(Creator, Name, modechar, PARAM_ALWAYS, MODETYPE_CHANNEL, MC_LIST),
	listnumeric(lnum), endoflistnumeric(eolnum), endofliststring(eolstr), tidy(autotidy),
	configtag(ctag)
	, extItem("listbase_mode_" + name + "_list", ExtensionItem::EXT_CHANNEL, Creator)
{
	list = true;
}

void ListModeBase::DisplayList(User* user, Channel* channel)
{
	ChanData* cd = extItem.get(channel);
	if (cd)
	{
		for (ModeList::const_iterator it = cd->list.begin(); it != cd->list.end(); ++it)
		{
			user->WriteNumeric(listnumeric, channel->name, it->mask, it->setter, (unsigned long) it->time);
		}
	}
	user->WriteNumeric(endoflistnumeric, channel->name, endofliststring);
}

void ListModeBase::DisplayEmptyList(User* user, Channel* channel)
{
	user->WriteNumeric(endoflistnumeric, channel->name, endofliststring);
}

void ListModeBase::RemoveMode(Channel* channel, Modes::ChangeList& changelist)
{
	ChanData* cd = extItem.get(channel);
	if (cd)
	{
		for (ModeList::iterator it = cd->list.begin(); it != cd->list.end(); it++)
		{
			changelist.push_remove(this, it->mask);
		}
	}
}

void ListModeBase::DoRehash()
{
	ConfigTagList tags = ServerInstance->Config->ConfTags(configtag);

	limitlist oldlimits = chanlimits;
	chanlimits.clear();

	for (ConfigIter i = tags.first; i != tags.second; i++)
	{
		// For each <banlist> tag
		ConfigTag* c = i->second;
		ListLimit limit(c->getString("chan"), c->getInt("limit"));

		if (limit.mask.size() && limit.limit > 0)
			chanlimits.push_back(limit);
	}

	// Add the default entry. This is inserted last so if the user specifies a
	// wildcard record in the config it will take precedence over this entry.
	chanlimits.push_back(ListLimit("*", DEFAULT_LIST_SIZE));

	// Most of the time our settings are unchanged, so we can avoid iterating the chanlist
	if (oldlimits == chanlimits)
		return;

	const chan_hash& chans = ServerInstance->GetChans();
	for (chan_hash::const_iterator i = chans.begin(); i != chans.end(); ++i)
	{
		ChanData* cd = extItem.get(i->second);
		if (cd)
			cd->maxitems = -1;
	}
}

unsigned int ListModeBase::FindLimit(const std::string& channame)
{
	for (limitlist::iterator it = chanlimits.begin(); it != chanlimits.end(); ++it)
	{
		if (InspIRCd::Match(channame, it->mask))
		{
			// We have a pattern matching the channel
			return it->limit;
		}
	}
	return DEFAULT_LIST_SIZE;
}

unsigned int ListModeBase::GetLimitInternal(const std::string& channame, ChanData* cd)
{
	if (cd->maxitems < 0)
		cd->maxitems = FindLimit(channame);
	return cd->maxitems;
}

unsigned int ListModeBase::GetLimit(Channel* channel)
{
	ChanData* cd = extItem.get(channel);
	if (!cd) // just find the limit
		return FindLimit(channel->name);

	return GetLimitInternal(channel->name, cd);
}

unsigned int ListModeBase::GetLowerLimit()
{
	unsigned int limit = UINT_MAX;
	for (limitlist::iterator iter = chanlimits.begin(); iter != chanlimits.end(); ++iter)
	{
		if (iter->limit < limit)
			limit = iter->limit;
	}
	return limit == UINT_MAX ? DEFAULT_LIST_SIZE : limit;
}

ModeAction ListModeBase::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	// Try and grab the list
	ChanData* cd = extItem.get(channel);

	if (adding)
	{
		if (tidy)
			ModeParser::CleanMask(parameter);

		if (parameter.length() > 250)
			return MODEACTION_DENY;

		// If there was no list
		if (!cd)
		{
			// Make one
			cd = new ChanData;
			extItem.set(channel, cd);
		}

		// Check if the item already exists in the list
		for (ModeList::iterator it = cd->list.begin(); it != cd->list.end(); it++)
		{
			if (parameter == it->mask)
			{
				/* Give a subclass a chance to error about this */
				TellAlreadyOnList(source, channel, parameter);

				// it does, deny the change
				return MODEACTION_DENY;
			}
		}

		if ((IS_LOCAL(source)) && (cd->list.size() >= GetLimitInternal(channel->name, cd)))
		{
			/* List is full, give subclass a chance to send a custom message */
			TellListTooLong(source, channel, parameter);
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
		if (ValidateParam(source, channel, parameter))
		{
			// And now add the mask onto the list...
			cd->list.push_back(ListItem(parameter, source->nick, ServerInstance->Time()));
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
				if (parameter == it->mask)
				{
					stdalgo::vector::swaperase(cd->list, it);
					return MODEACTION_ALLOW;
				}
			}
		}

		/* Tried to remove something that wasn't set */
		TellNotSet(source, channel, parameter);
		return MODEACTION_DENY;
	}
}

bool ListModeBase::ValidateParam(User*, Channel*, std::string&)
{
	return true;
}

void ListModeBase::TellListTooLong(User* source, Channel* channel, std::string& parameter)
{
	source->WriteNumeric(ERR_BANLISTFULL, channel->name, parameter, "Channel ban list is full");
}

void ListModeBase::TellAlreadyOnList(User*, Channel*, std::string&)
{
}

void ListModeBase::TellNotSet(User*, Channel*, std::string&)
{
}
