/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "xline.h"

#include "commands.h"

CmdResult CommandDelLine::Handle(User* user, Params& params) {
    const std::string& setter = user->nick;
    std::string reason;

    // XLineManager::DelLine() returns true if the xline existed, false if it didn't
    if (ServerInstance->XLines->DelLine(params[1].c_str(), params[0], reason,
                                        user)) {
        ServerInstance->SNO->WriteToSnoMask('X', "%s removed %s%s on %s: %s",
                                            setter.c_str(),
                                            params[0].c_str(), params[0].length() == 1 ? "-line" : "", params[1].c_str(),
                                            reason.c_str());
        return CMD_SUCCESS;
    }
    return CMD_FAILURE;
}
