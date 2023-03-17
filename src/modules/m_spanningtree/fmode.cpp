/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
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
CmdResult CommandFMode::Handle(User* who, Params& params) {
    time_t TS = ServerCommand::ExtractTS(params[1]);

    Channel* const chan = ServerInstance->FindChan(params[0]);
    if (!chan)
        // Channel doesn't exist
    {
        return CMD_FAILURE;
    }

    // Extract the TS of the channel in question
    time_t ourTS = chan->age;

    /* If the TS is greater than ours, we drop the mode and don't pass it anywhere.
     */
    if (TS > ourTS) {
        return CMD_FAILURE;
    }

    /* TS is equal or less: apply the mode change locally and forward the message
     */

    // Turn modes into a Modes::ChangeList; may have more elements than max modes
    Modes::ChangeList changelist;
    ServerInstance->Modes.ModeParamsToChangeList(who, MODETYPE_CHANNEL, params,
            changelist, 2);

    ModeParser::ModeProcessFlag flags = ModeParser::MODE_LOCALONLY;
    if ((TS == ourTS) && IS_SERVER(who)) {
        flags |= ModeParser::MODE_MERGE;
    }

    ServerInstance->Modes->Process(who, chan, NULL, changelist, flags);
    return CMD_SUCCESS;
}
