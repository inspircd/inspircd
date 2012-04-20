/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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
#include "modes/cmode_l.h"

ModeChannelLimit::ModeChannelLimit(InspIRCd* Instance) : ModeHandler(Instance, 'l', 1, 0, false, MODETYPE_CHANNEL, false)
{
}

ModePair ModeChannelLimit::ModeSet(User*, User*, Channel* channel, const std::string &parameter)
{
	std::string climit = channel->GetModeParameter('l');
	if (!climit.empty())
	{
		return std::make_pair(true, climit);
	}
	else
	{
		return std::make_pair(false, parameter);
	}
}

bool ModeChannelLimit::CheckTimeStamp(time_t, time_t, const std::string &their_param, const std::string &our_param, Channel*)
{
	/* When TS is equal, the higher channel limit wins */
	return (atoi(their_param.c_str()) < atoi(our_param.c_str()));
}

ModeAction ModeChannelLimit::OnModeChange(User*, User*, Channel* channel, std::string &parameter, bool adding, bool servermode)
{
	if (adding)
	{
		/* Setting a new limit, sanity check */
		long limit = atoi(parameter.c_str());

		/* Wrap low values at 32768 */
		if (limit < 0)
			limit = 0x7FFF;

		parameter = ConvToStr(limit);

		std::string now = channel->GetModeParameter('l');

		if (now == parameter)
			return MODEACTION_DENY;

		/* Set new limit */
		channel->SetModeParam('l', parameter);

		return MODEACTION_ALLOW;
	}
	else
	{
		/* Check if theres a limit here to remove.
		 * If there isnt, dont allow the -l
		 */
		if (channel->GetModeParameter('l').empty())
			return MODEACTION_DENY;

		/* Removing old limit, no checks here */
		channel->SetModeParam('l', "");
		return MODEACTION_ALLOW;
	}
}
