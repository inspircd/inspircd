/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

enum {
    // From ircu.
    ERR_INVALIDUSERNAME = 468
};

CommandUser::CommandUser(Module* parent)
    : SplitCommand(parent, "USER", 4, 4) {
    allow_empty_last_param = false;
    works_before_reg = true;
    Penalty = 0;
    syntax = "<username> <unused> <unused> :<realname>";
}

CmdResult CommandUser::HandleLocal(LocalUser* user, const Params& parameters) {
    /* A user may only send the USER command once */
    if (!(user->registered & REG_USER)) {
        if (!ServerInstance->IsIdent(parameters[0])) {
            user->WriteNumeric(ERR_INVALIDUSERNAME, name, "Your username is not valid");
            return CMD_FAILURE;
        } else {
            user->ChangeIdent(parameters[0]);
            user->ChangeRealName(parameters[3]);
            user->registered = (user->registered | REG_USER);
        }
    } else {
        user->WriteNumeric(ERR_ALREADYREGISTERED, "You may not reregister");
        user->CommandFloodPenalty += 1000;
        return CMD_FAILURE;
    }

    /* parameters 2 and 3 are local and remote hosts, and are ignored */
    return CheckRegister(user);
}

CmdResult CommandUser::CheckRegister(LocalUser* user) {
    // If the user is registered, return CMD_SUCCESS/CMD_FAILURE depending on what modules say, otherwise just
    // return CMD_SUCCESS without doing anything, knowing the other handler will call us again
    if (user->registered == REG_NICKUSER) {
        ModResult MOD_RESULT;
        FIRST_MOD_RESULT(OnUserRegister, MOD_RESULT, (user));
        if (MOD_RESULT == MOD_RES_DENY) {
            return CMD_FAILURE;
        }
    }

    return CMD_SUCCESS;
}
