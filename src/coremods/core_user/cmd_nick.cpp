/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2016, 2018, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007 Craig Edwards <brain@inspircd.org>
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

CommandNick::CommandNick(Module* parent)
    : SplitCommand(parent, "NICK", 1) {
    works_before_reg = true;
    syntax = "<newnick>";
    Penalty = 0;
}

/** Handle nick changes from users.
 * NOTE: If you are used to ircds based on ircd2.8, and are looking
 * for the client introduction code in here, youre in the wrong place.
 * You need to look in the spanningtree module for this!
 */
CmdResult CommandNick::HandleLocal(LocalUser* user, const Params& parameters) {
    std::string newnick = parameters[0];

    // anything except the initial NICK gets a flood penalty
    if (user->registered == REG_ALL) {
        user->CommandFloodPenalty += 4000;
    }

    if (newnick.empty()) {
        user->WriteNumeric(ERR_NONICKNAMEGIVEN, "No nickname given");
        return CMD_FAILURE;
    }

    if (newnick == "0") {
        newnick = user->uuid;
    } else if (!ServerInstance->IsNick(newnick)) {
        user->WriteNumeric(ERR_ERRONEUSNICKNAME, newnick, "Erroneous Nickname");
        return CMD_FAILURE;
    }

    ModResult MOD_RESULT;
    FIRST_MOD_RESULT(OnUserPreNick, MOD_RESULT, (user, newnick));

    // If a module denied the change, abort now
    if (MOD_RESULT == MOD_RES_DENY) {
        return CMD_FAILURE;
    }

    // Disallow the nick change if <security:restrictbannedusers> is on and there is a ban matching this user in
    // one of the channels they are on
    if (ServerInstance->Config->RestrictBannedUsers != ServerConfig::BUT_NORMAL) {
        for (User::ChanList::iterator i = user->chans.begin(); i != user->chans.end();
                ++i) {
            Channel* chan = (*i)->chan;
            if (chan->GetPrefixValue(user) < VOICE_VALUE && chan->IsBanned(user)) {
                if (ServerInstance->Config->RestrictBannedUsers ==
                        ServerConfig::BUT_RESTRICT_NOTIFY)
                    user->WriteNumeric(ERR_CANTCHANGENICK,
                                       InspIRCd::Format("Cannot change nickname while on %s (you're banned)",
                                                        chan->name.c_str()));
                return CMD_FAILURE;
            }
        }
    }

    if (!user->ChangeNick(newnick)) {
        return CMD_FAILURE;
    }

    if (user->registered < REG_NICKUSER) {
        user->registered = (user->registered | REG_NICK);
        return CommandUser::CheckRegister(user);
    }

    return CMD_SUCCESS;
}
