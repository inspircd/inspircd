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
#include "modes/cmode_t.h"

ModeChannelTopicOps::ModeChannelTopicOps(InspIRCd* Instance) : ModeHandler(Instance, 't', 0, 0, false, MODETYPE_CHANNEL, false)
{
}

ModeAction ModeChannelTopicOps::OnModeChange(User*, User*, Channel* channel, std::string&, bool adding, bool servermode)
{
	if (channel->modes[CM_TOPICLOCK] != adding)
	{
		channel->modes[CM_TOPICLOCK] = adding;
		return MODEACTION_ALLOW;
	}
	else
	{
		return MODEACTION_DENY;
	}
}

