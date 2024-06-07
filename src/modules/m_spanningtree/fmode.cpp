/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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
#include "commands.h"

/** FMODE command - channel mode change with timestamp checks */
CmdResult CommandFMode::Handle(User* who, Params& params)
{
	time_t TS = ServerCommand::ExtractTS(params[1]);

	Channel* const chan = ServerInstance->Channels.Find(params[0]);
	if (!chan)
		// Channel doesn't exist
		return CmdResult::FAILURE;

	// Extract the TS of the channel in question
	time_t ourTS = chan->age;

	/* If the TS is greater than ours, we drop the mode and don't pass it anywhere.
	 */
	if (TS > ourTS)
		return CmdResult::FAILURE;

	/* TS is equal or less: apply the mode change locally and forward the message
	 */

	// Turn modes into a Modes::ChangeList; may have more elements than max modes
	Modes::ChangeList changelist;
	ServerInstance->Modes.ModeParamsToChangeList(who, MODETYPE_CHANNEL, params, changelist, 2);

	ModeParser::ModeProcessFlag flags = ModeParser::MODE_LOCALONLY;
	if ((TS == ourTS) && IS_SERVER(who))
		flags |= ModeParser::MODE_MERGE;

	ServerInstance->Modes.Process(who, chan, nullptr, changelist, flags);
	return CmdResult::SUCCESS;
}

CmdResult CommandLMode::Handle(User* who, Params& params)
{
	// :<sid> LMODE <chan> <chants> <modechr> [<mask> <setts> <setter>]+
	time_t chants = ServerCommand::ExtractTS(params[1]);

	Channel* const chan = ServerInstance->Channels.Find(params[0]);
	if (!chan)
		return CmdResult::FAILURE; // Channel doesn't exist.

	// If the TS is greater than ours, we drop the mode and don't pass it anywhere.
	if (chants > chan->age)
		return CmdResult::FAILURE;

	ModeHandler* mh = ServerInstance->Modes.FindMode(params[2][0], MODETYPE_CHANNEL);
	if (!mh || !mh->IsListMode())
		return CmdResult::FAILURE; // Mode doesn't exist or isn't a list mode.

	if (params.size() % 3)
		return CmdResult::FAILURE; // Invalid parameter count.

	Modes::ChangeList changelist;
	for (Params::const_iterator iter = params.begin() + 3; iter != params.end(); )
	{
		// The mode mask (e.g. foo!bar@baz).
		const std::string& mask = *iter++;

		// Who the mode was set by (e.g. Sadie!sadie@sadie.moe).
		const std::string& set_by = *iter++;

		// The time at which the mode was set (e.g. 956204400).
		time_t set_at = ServerCommand::ExtractTS(*iter++);

		changelist.push(mh, true, mask, set_by, set_at);
	}

	ModeParser::ModeProcessFlag flags = ModeParser::MODE_LOCALONLY;
	if (chants == chan->age && IS_SERVER(who))
		flags |= ModeParser::MODE_MERGE;

	ServerInstance->Modes.Process(who, chan, nullptr, changelist, flags);
	return CmdResult::SUCCESS;
}
