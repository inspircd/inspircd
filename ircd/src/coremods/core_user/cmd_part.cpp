/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
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
#include "core_user.h"

CommandPart::CommandPart(Module* parent)
    : Command(parent, "PART", 1, 2) {
    Penalty = 5;
    syntax = "<channel>[,<channel>]+ [:<reason>]";
}

CmdResult CommandPart::Handle(User* user, const Params& parameters) {
    std::string reason;
    if (parameters.size() > 1) {
        if (IS_LOCAL(user)) {
            msgwrap.Wrap(parameters[1], reason);
        } else {
            reason = parameters[1];
        }
    }

    if (CommandParser::LoopCall(user, this, parameters, 0)) {
        return CMD_SUCCESS;
    }

    Channel* c = ServerInstance->FindChan(parameters[0]);

    if (!c) {
        user->WriteNumeric(Numerics::NoSuchChannel(parameters[0]));
        return CMD_FAILURE;
    }

    if (!c->PartUser(user, reason)) {
        user->WriteNumeric(ERR_NOTONCHANNEL, c->name, "You're not on that channel");
        return CMD_FAILURE;
    }

    return CMD_SUCCESS;
}

RouteDescriptor CommandPart::GetRouting(User* user, const Params& parameters) {
    return (IS_LOCAL(user) ? ROUTE_LOCALONLY : ROUTE_BROADCAST);
}
