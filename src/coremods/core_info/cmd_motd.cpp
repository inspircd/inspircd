/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014, 2018-2020, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

CommandMotd::CommandMotd(Module* parent)
    : ServerTargetCommand(parent, "MOTD") {
    syntax = "[<servername>]";
}

/** Handle /MOTD
 */
CmdResult CommandMotd::Handle(User* user, const Params& parameters) {
    if (parameters.size() > 0
            && !irc::equals(parameters[0], ServerInstance->Config->ServerName)) {
        // Give extra penalty if a non-oper queries the /MOTD of a remote server
        LocalUser* localuser = IS_LOCAL(user);
        if ((localuser) && (!user->IsOper())) {
            localuser->CommandFloodPenalty += 2000;
        }
        return CMD_SUCCESS;
    }

    ConfigTag* tag = ServerInstance->Config->EmptyTag;
    LocalUser* localuser = IS_LOCAL(user);
    if (localuser) {
        tag = localuser->GetClass()->config;
    }

    const std::string motd_name = tag->getString("motd", "motd", 1);
    ConfigFileCache::iterator motd = motds.find(motd_name);
    if (motd == motds.end()) {
        user->WriteRemoteNumeric(ERR_NOMOTD, "Message of the day file is missing.");
        return CMD_SUCCESS;
    }

    user->WriteRemoteNumeric(RPL_MOTDSTART,
                             InspIRCd::Format("%s message of the day",
                                     ServerInstance->Config->GetServerName().c_str()));
    for (file_cache::iterator i = motd->second.begin(); i != motd->second.end();
            i++) {
        user->WriteRemoteNumeric(RPL_MOTD, *i);
    }
    user->WriteRemoteNumeric(RPL_ENDOFMOTD, "End of message of the day.");

    return CMD_SUCCESS;
}
