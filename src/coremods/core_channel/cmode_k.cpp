/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

enum
{
	// From RFC 1459.
	ERR_KEYSET = 467,
};

ModeChannelKey::ModeChannelKey(Module* Creator)
	: ParamMode<ModeChannelKey, StringExtItem>(Creator, "key", 'k', PARAM_ALWAYS)
{
	syntax = "<key>";
}

bool ModeChannelKey::OnModeChange(User* source, User*, Channel* channel, Modes::Change& change)
{
	const std::string* key = ext.Get(channel);
	bool exists = (key != nullptr);
	if (IS_LOCAL(source))
	{
		if (exists == change.adding)
			return false;
		if (exists && (change.param != *key))
		{
			/* Key is currently set and the correct key wasn't given */
			source->WriteNumeric(ERR_KEYSET, channel->name, "Channel key already set");
			return false;
		}
	} else {
		if (exists && change.adding && change.param == *key)
		{
			/* no-op, don't show */
			return false;
		}
	}

	if (change.adding)
	{
		// When joining a channel multiple keys are delimited with a comma so we strip
		// them out here to avoid creating channels that are unjoinable.
		size_t commapos;
		while ((commapos = change.param.find(',')) != std::string::npos)
			change.param.erase(commapos, 1);

		// Truncate the parameter to the maximum key length.
		if (change.param.length() > maxkeylen)
			change.param.erase(maxkeylen);

		// If the password is empty here then it only consisted of commas. This is not
		// acceptable so we reject the mode change.
		if (change.param.empty())
			return false;

		ext.Set(channel, change.param);
	}
	else
		ext.Unset(channel);

	channel->SetMode(this, change.adding);
	return true;
}

void ModeChannelKey::SerializeParam(Channel* chan, const std::string* key, std::string& out)
{
	out += *key;
}

bool ModeChannelKey::OnSet(User* source, Channel* chan, std::string& param)
{
	// Dummy function, never called
	return false;
}

bool ModeChannelKey::IsParameterSecret()
{
	return true;
}
