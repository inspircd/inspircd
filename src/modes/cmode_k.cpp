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
#include "mode.h"
#include "channels.h"
#include "users.h"
#include "modes/cmode_k.h"

ModeChannelKey::ModeChannelKey(InspIRCd* Instance) : ModeHandler(Instance, 'k', 1, 1, false, MODETYPE_CHANNEL, false)
{
}

ModePair ModeChannelKey::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
    if (channel->modes[CM_KEY])
    {
		std::string ckey = channel->GetModeParameter('k');
		return std::make_pair(true, ckey);
    }
    else
    {
		return std::make_pair(false, parameter);
    }
}

void ModeChannelKey::RemoveMode(Channel* channel, irc::modestacker* stack)
{
	/** +k needs a parameter when being removed,
	 * so we have a special-case RemoveMode here for it
	 */

	if (channel->IsModeSet('k'))
	{
		if (stack)
		{
			stack->Push('k', channel->GetModeParameter('k'));
		}
		else
		{
			std::vector<std::string> parameters;
			parameters.push_back(channel->name);
			parameters.push_back("-k");
			parameters.push_back(channel->GetModeParameter('k'));
			ServerInstance->SendMode(parameters, ServerInstance->FakeClient);
		}
	}
}

void ModeChannelKey::RemoveMode(User*, irc::modestacker* stack)
{
}

bool ModeChannelKey::CheckTimeStamp(time_t, time_t, const std::string &their_param, const std::string &our_param, Channel*)
{
	/* When TS is equal, the alphabetically later channel key wins */
	return (their_param < our_param);
}

ModeAction ModeChannelKey::OnModeChange(User* source, User*, Channel* channel, std::string &parameter, bool adding, bool servermode)
{
	bool exists = channel->IsModeSet('k');
	if (IS_LOCAL(source))
	{
		if (exists == adding)
			return MODEACTION_DENY;
		if (exists && (parameter != channel->GetModeParameter('k')))
		{
			/* Key is currently set and the correct key wasnt given */
			return MODEACTION_DENY;
		}
	} else {
		if (exists && adding && parameter == channel->GetModeParameter('k'))
		{
			/* no-op, don't show */
			return MODEACTION_DENY;
		}
	}

	/* invalid keys */
	if (!parameter.length())
		return MODEACTION_DENY;

	if (parameter.rfind(' ') != std::string::npos)
		return MODEACTION_DENY;

	if (adding)
	{
		std::string ckey;
		ckey.assign(parameter, 0, 32);
		parameter = ckey;
		channel->SetModeParam('k', parameter);
	}
	else
	{
		channel->SetModeParam('k', "");
	}
	return MODEACTION_ALLOW;
}
