/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
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

CommandAdmin::CommandAdmin(Module* parent)
    : ServerTargetCommand(parent, "ADMIN") {
    Penalty = 2;
    syntax = "[<servername>]";
}

/** Handle /ADMIN
 */
CmdResult CommandAdmin::Handle(User* user, const Params& parameters) {
    if (parameters.size() > 0
            && !irc::equals(parameters[0], ServerInstance->Config->ServerName)) {
        return CMD_SUCCESS;
    }
    user->WriteRemoteNumeric(RPL_ADMINME, ServerInstance->Config->GetServerName(),
                             "Administrative info");
    if (!AdminName.empty()) {
        user->WriteRemoteNumeric(RPL_ADMINLOC1, InspIRCd::Format("Name: %s",
                                 AdminName.c_str()));
    }
    user->WriteRemoteNumeric(RPL_ADMINLOC2, InspIRCd::Format("Nickname: %s",
                             AdminNick.c_str()));
    user->WriteRemoteNumeric(RPL_ADMINEMAIL, InspIRCd::Format("Email: %s",
                             AdminEmail.c_str()));
    return CMD_SUCCESS;
}
