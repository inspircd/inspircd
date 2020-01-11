/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2010 Craig Edwards <brain@inspircd.org>
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

ModeChannelLimit::ModeChannelLimit(Module* Creator)
	: ParamMode<ModeChannelLimit, LocalIntExt>(Creator, "limit", 'l')
	, minlimit(0)
{
	syntax = "<limit>";
}

bool ModeChannelLimit::ResolveModeConflict(std::string &their_param, const std::string &our_param, Channel*)
{
	/* When TS is equal, the higher channel limit wins */
	return ConvToNum<intptr_t>(their_param) < ConvToNum<intptr_t>(our_param);
}

ModeAction ModeChannelLimit::OnSet(User* user, Channel* chan, std::string& parameter)
{
	size_t limit = ConvToNum<size_t>(parameter);
	if (limit < minlimit)
		return MODEACTION_DENY;

	ext.set(chan, limit);
	return MODEACTION_ALLOW;
}

void ModeChannelLimit::SerializeParam(Channel* chan, intptr_t n, std::string& out)
{
	out += ConvToStr(n);
}
