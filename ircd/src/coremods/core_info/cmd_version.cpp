/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
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
#include "core_info.h"

CommandVersion::CommandVersion(Module* parent)
    : Command(parent, "VERSION", 0, 0) {
    syntax = "[<servername>]";
}

CmdResult CommandVersion::Handle(User* user, const Params& parameters) {
    Numeric::Numeric numeric(RPL_VERSION);
    irc::tokenstream tokens(ServerInstance->GetVersionString(user->IsOper()));
    for (std::string token; tokens.GetTrailing(token); ) {
        numeric.push(token);
    }
    user->WriteNumeric(numeric);

    LocalUser *lu = IS_LOCAL(user);
    if (lu != NULL) {
        ServerInstance->ISupport.SendTo(lu);
    }
    return CMD_SUCCESS;
}
