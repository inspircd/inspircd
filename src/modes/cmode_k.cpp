/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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
#include "builtinmodes.h"

ModeChannelKey::ModeChannelKey()
	: ParamMode<ModeChannelKey, LocalStringExt>(NULL, "key", 'k', PARAM_ALWAYS)
{
}

ModeAction ModeChannelKey::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding)
{
	const std::string* key = ext.get(channel);
	bool exists = (key != NULL);
	if (IS_LOCAL(source))
	{
		if (exists == adding)
			return MODEACTION_DENY;
		if (exists && (parameter != *key))
		{
			/* Key is currently set and the correct key wasnt given */
			return MODEACTION_DENY;
		}
	} else {
		if (exists && adding && parameter == *key)
		{
			/* no-op, don't show */
			return MODEACTION_DENY;
		}
	}

	channel->SetMode(this, adding);
	if (adding)
	{
		if (parameter.length() > maxkeylen)
			parameter.erase(maxkeylen);
		ext.set(channel, parameter);
	}
	else
		ext.unset(channel);

	return MODEACTION_ALLOW;
}

void ModeChannelKey::SerializeParam(Channel* chan, const std::string* key, std::string& out)
{
	out += *key;
}

ModeAction ModeChannelKey::OnSet(User* source, Channel* chan, std::string& param)
{
	// Dummy function, never called
	return MODEACTION_DENY;
}
