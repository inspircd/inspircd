/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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

ModeUserOperator::ModeUserOperator(Module* Creator)
    : ModeHandler(Creator, "oper", 'o', PARAM_NONE, MODETYPE_USER) {
    oper = true;
}

ModeAction ModeUserOperator::OnModeChange(User* source, User* dest, Channel*,
        std::string&, bool adding) {
    /* Only opers can execute this class at all */
    if (!source->server->IsULine() && !source->IsOper()) {
        return MODEACTION_DENY;
    }

    /* Not even opers can GIVE the +o mode, only take it away */
    if (adding) {
        return MODEACTION_DENY;
    }

    /* Set the bitfields.
     * Note that oper status is only given in User::Oper()
     * NOT here. It is impossible to directly set +o without
     * verifying as an oper and getting an opertype assigned
     * to your User!
     */
    char snomask = IS_LOCAL(dest) ? 'o' : 'O';
    ServerInstance->SNO->WriteToSnoMask(snomask, "User %s de-opered (by %s)",
                                        dest->nick.c_str(), source->nick.c_str());
    dest->UnOper();

    return MODEACTION_ALLOW;
}
