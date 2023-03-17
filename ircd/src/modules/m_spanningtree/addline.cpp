/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018, 2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
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

#include "treeserver.h"
#include "utils.h"
#include "commands.h"

CmdResult CommandAddLine::Handle(User* usr, Params& params) {
    XLineFactory* xlf = ServerInstance->XLines->GetFactory(params[0]);
    const std::string& setter = usr->nick;

    if (!xlf) {
        ServerInstance->SNO->WriteToSnoMask('x',
                                            "%s sent me an unknown ADDLINE type (%s).", setter.c_str(), params[0].c_str());
        return CMD_FAILURE;
    }

    XLine* xl = NULL;
    try {
        xl = xlf->Generate(ServerInstance->Time(), ConvToNum<unsigned long>(params[4]),
                           params[2], params[5], params[1]);
    } catch (ModuleException &e) {
        ServerInstance->SNO->WriteToSnoMask('x',
                                            "Unable to ADDLINE type %s from %s: %s", params[0].c_str(), setter.c_str(),
                                            e.GetReason().c_str());
        return CMD_FAILURE;
    }
    xl->SetCreateTime(ConvToNum<time_t>(params[3]));
    if (ServerInstance->XLines->AddLine(xl, NULL)) {
        if (xl->duration) {
            ServerInstance->SNO->WriteToSnoMask('X',
                                                "%s added a timed %s%s on %s, expires in %s (on %s): %s",
                                                setter.c_str(), params[0].c_str(), params[0].length() == 1 ? "-line" : "",
                                                params[1].c_str(), InspIRCd::DurationString(xl->duration).c_str(),
                                                InspIRCd::TimeString(xl->expiry).c_str(), params[5].c_str());
        } else {
            ServerInstance->SNO->WriteToSnoMask('X', "%s added a permanent %s%s on %s: %s",
                                                setter.c_str(), params[0].c_str(), params[0].length() == 1 ? "-line" : "",
                                                params[1].c_str(), params[5].c_str());
        }

        TreeServer* remoteserver = TreeServer::Get(usr);

        if (!remoteserver->IsBursting()) {
            ServerInstance->XLines->ApplyLines();
        }
        return CMD_SUCCESS;
    } else {
        delete xl;
        return CMD_FAILURE;
    }
}

CommandAddLine::Builder::Builder(XLine* xline, User* user)
    : CmdBuilder(user, "ADDLINE") {
    push(xline->type);
    push(xline->Displayable());
    push(xline->source);
    push_int(xline->set_time);
    push_int(xline->duration);
    push_last(xline->reason);
}
