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
	: ModeHandler(Creator, Name, modechar, PARAM_ALWAYS, MODETYPE_CHANNEL),
	listnumeric(lnum), endoflistnumeric(eolnum), endofliststring(eolstr), tidy(autotidy),
	configtag(ctag), extItem("listbase_mode_" + name + "_list", Creator)
{
	list = true;
}

void ListModeBase::DisplayList(User* user, Channel* channel)
{
	ChanData* cd = extItem.get(channel);
	if (cd)
	{
		for (ModeList::reverse_iterator it = cd->list.rbegin(); it != cd->list.rend(); ++it)
		{
			user->WriteNumeric(listnumeric, "%s %s %s %s %lu", user->nick.c_str(), channel->name.c_str(), it->mask.c_str(), (!it->setter.empty() ? it->setter.c_str() : ServerInstance->Config->ServerName.c_str()), (unsigned long) it->time);
		}
	}
	user->WriteNumeric(endoflistnumeric, "%s %s :%s", user->nick.c_str(), channel->name.c_str(), endofliststring.c_str());
}

void ListModeBase::DisplayEmptyList(User* user, Channel* channel)
{
	user->WriteNumeric(endoflistnumeric, "%s %s :%s", user->nick.c_str(), channel->name.c_str(), endofliststring.c_str());
}

void ListModeBase::RemoveMode(Channel* channel, irc::modestacker& stack)
{
	ChanData* cd = extItem.get(channel);
	if (cd)
	{
		for (ModeList::iterator it = cd->list.begin(); it != cd->list.end(); it++)
		{
			stack.Push(this->GetModeChar(), it->mask);
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

	if (chanlimits.empty())
		chanlimits.push_back(ListLimit("*", 64));

	// Most of the time our settings are unchanged, so we can avoid iterating the chanlist
	if (oldlimits == chanlimits)
		return;

	for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); ++i)
	{
		ChanData* cd = extItem.get(i->second);
		if (cd)
			cd->maxitems = -1;
	}
}

void ListModeBase::DoImplements(Module* m)
{
	ServerInstance->Modules->AddService(extItem);
	this->DoRehash();
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
	return 64;
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
			parameter.clear();
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
					cd->list.erase(it);
					return MODEACTION_ALLOW;
				}
			}
		}

		/* Tried to remove something that wasn't set */
		TellNotSet(source, channel, parameter);
		parameter.clear();
		return MODEACTION_DENY;
	}
}

void ListModeBase::DoSyncChannel(Channel* chan, Module* proto, void* opaque)
{
	ChanData* cd = extItem.get(chan);
	if (!cd)
		return;

	irc::modestacker modestack(true);
	std::vector<std::string> stackresult;
	std::vector<TranslateType> types;
	types.push_back(TR_TEXT);

	for (ModeList::iterator it = cd->list.begin(); it != cd->list.end(); it++)
		modestack.Push(mode, it->mask);

	while (modestack.GetStackedLine(stackresult))
	{
		types.assign(stackresult.size(), this->GetTranslateType());
		proto->ProtoSendMode(opaque, TYPE_CHANNEL, chan, stackresult, types);
		stackresult.clear();
	}
}

bool ListModeBase::ValidateParam(User*, Channel*, std::string&)
{
	return true;
}

void ListModeBase::TellListTooLong(User* source, Channel* channel, std::string& parameter)
{
	source->WriteNumeric(478, "%s %s %s :Channel ban list is full", source->nick.c_str(), channel->name.c_str(), parameter.c_str());
}

void ListModeBase::TellAlreadyOnList(User*, Channel*, std::string&)
{
}

void ListModeBase::TellNotSet(User*, Channel*, std::string&)
{
}
