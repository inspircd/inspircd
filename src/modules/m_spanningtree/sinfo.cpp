/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
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

#include "treeserver.h"
#include "commands.h"

CmdResult CommandSInfo::HandleServer(TreeServer* server,
                                     CommandBase::Params& params) {
    const std::string& key = params.front();
    const std::string& value = params.back();

    if (key == "fullversion") {
        server->SetFullVersion(value);
    } else if (key == "version") {
        server->SetVersion(value);
    } else if (key == "rawversion") {
        server->SetRawVersion(value);
    } else if (key == "desc") {
        // Only sent when the description of a server changes because of a rehash; not sent on burst
        ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                  "Server description of " + server->GetName() + " changed: " + value);
        server->SetDesc(value);
    }

    return CMD_SUCCESS;
}

CommandSInfo::Builder::Builder(TreeServer* server, const char* key,
                               const std::string& val)
    : CmdBuilder(server, "SINFO") {
    push(key).push_last(val);
}
