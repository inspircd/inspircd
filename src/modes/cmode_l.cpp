/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

ModeChannelLimit::ModeChannelLimit() : ParamChannelModeHandler(NULL, "limit", 'l')
{
}

bool ModeChannelLimit::ResolveModeConflict(std::string &their_param, const std::string &our_param, Channel*)
{
	/* When TS is equal, the higher channel limit wins */
	return (atoi(their_param.c_str()) < atoi(our_param.c_str()));
}

bool ModeChannelLimit::ParamValidate(std::string &parameter)
{
	int limit = atoi(parameter.c_str());

	if (limit < 0)
		return false;

	parameter = ConvToStr(limit);
	return true;
}
