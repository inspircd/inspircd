/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Val Lorentz <progval+git@progval.net>
 *   Copyright (C) 2018-2019 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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
#include "core_channel.h"
#include "modules/names.h"

CommandNames::CommandNames(Module* parent)
    : SplitCommand(parent, "NAMES", 0, 0)
    , secretmode(parent, "secret")
    , privatemode(parent, "private")
    , invisiblemode(parent, "invisible")
    , namesevprov(parent, "event/names") {
    syntax = "[<channel>[,<channel>]+]";
}

/** Handle /NAMES
 */
CmdResult CommandNames::HandleLocal(LocalUser* user, const Params& parameters) {
    Channel* c;

    if (parameters.empty()) {
        user->WriteNumeric(RPL_ENDOFNAMES, '*', "End of /NAMES list.");
        return CMD_SUCCESS;
    }

    if (CommandParser::LoopCall(user, this, parameters, 0)) {
        return CMD_SUCCESS;
    }

    c = ServerInstance->FindChan(parameters[0]);
    if (c) {
        // Show the NAMES list if one of the following is true:
        // - the channel is not secret
        // - the user doing the /NAMES is inside the channel
        // - the user doing the /NAMES has the channels/auspex privilege

        // If the user is inside the channel or has privs, instruct SendNames() to show invisible (+i) members
        bool show_invisible = ((c->HasUser(user))
                               || (user->HasPrivPermission("channels/auspex")));
        if ((show_invisible) || (!c->IsModeSet(secretmode))) {
            SendNames(user, c, show_invisible);
            return CMD_SUCCESS;
        }
    }

    user->WriteNumeric(RPL_ENDOFNAMES, parameters[0], "End of /NAMES list.");
    return CMD_SUCCESS;
}

void CommandNames::SendNames(LocalUser* user, Channel* chan,
                             bool show_invisible) {
    Numeric::Builder<' '> reply(user, RPL_NAMREPLY, false, chan->name.size() + 3);
    Numeric::Numeric& numeric = reply.GetNumeric();
    if (chan->IsModeSet(secretmode)) {
        numeric.push(std::string(1, '@'));
    } else if (chan->IsModeSet(privatemode)) {
        numeric.push(std::string(1, '*'));
    } else {
        numeric.push(std::string(1, '='));
    }

    numeric.push(chan->name);
    numeric.push(std::string());

    std::string prefixlist;
    std::string nick;
    const Channel::MemberMap& members = chan->GetUsers();
    for (Channel::MemberMap::const_iterator i = members.begin(); i != members.end();
            ++i) {
        if ((!show_invisible) && (i->first->IsModeSet(invisiblemode))) {
            // Member is invisible and we are not supposed to show them
            continue;
        }

        Membership* const memb = i->second;

        prefixlist.clear();
        char prefix = memb->GetPrefixChar();
        if (prefix) {
            prefixlist.push_back(prefix);
        }
        nick = i->first->nick;

        ModResult res;
        FIRST_MOD_RESULT_CUSTOM(namesevprov, Names::EventListener, OnNamesListItem, res,
                                (user, memb, prefixlist, nick));
        if (res != MOD_RES_DENY) {
            reply.Add(prefixlist, nick);
        }
    }

    reply.Flush();
    user->WriteNumeric(RPL_ENDOFNAMES, chan->name, "End of /NAMES list.");
}
