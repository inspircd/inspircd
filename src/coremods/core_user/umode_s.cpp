/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Robin Burchell <robin+git@viroteck.net>
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

ModeUserServerNoticeMask::ModeUserServerNoticeMask(Module* Creator)
    : ModeHandler(Creator, "snomask", 's', PARAM_SETONLY, MODETYPE_USER) {
    oper = true;
    syntax = "(+|-)<snomasks>|*";
}

ModeAction ModeUserServerNoticeMask::OnModeChange(User* source, User* dest,
        Channel*, std::string &parameter, bool adding) {
    if (adding) {
        dest->SetMode(this, true);
        // Process the parameter (remove chars we don't understand, remove redundant chars, etc.)
        parameter = ProcessNoticeMasks(dest, parameter);
        return MODEACTION_ALLOW;
    } else {
        if (dest->IsModeSet(this)) {
            dest->SetMode(this, false);
            dest->snomasks.reset();
            return MODEACTION_ALLOW;
        }
    }

    // Mode not set and trying to unset, deny
    return MODEACTION_DENY;
}

std::string ModeUserServerNoticeMask::GetUserParameter(const User* user) const {
    std::string ret;
    if (!user->IsModeSet(this)) {
        return ret;
    }

    ret.push_back('+');
    for (unsigned char n = 0; n < 64; n++) {
        if (user->snomasks[n]) {
            ret.push_back(n + 'A');
        }
    }
    return ret;
}

std::string ModeUserServerNoticeMask::ProcessNoticeMasks(User* user,
        const std::string& input) {
    bool adding = true;
    std::bitset<64> curr = user->snomasks;

    for (std::string::const_iterator i = input.begin(); i != input.end(); ++i) {
        switch (*i) {
        case '+':
            adding = true;
            break;
        case '-':
            adding = false;
            break;
        case '*':
            for (size_t j = 0; j < 64; j++) {
                const char chr = j + 'A';
                if (user->HasSnomaskPermission(chr)
                        && ServerInstance->SNO->IsSnomaskUsable(chr)) {
                    curr[j] = adding;
                }
            }
            break;
        default:
            // For local users check whether the given snomask is valid and enabled - IsSnomaskUsable() tests both.
            // For remote users accept what we were told, unless the snomask char is not a letter.
            if (IS_LOCAL(user)) {
                if (!ServerInstance->SNO->IsSnomaskUsable(*i)) {
                    user->WriteNumeric(ERR_UNKNOWNSNOMASK, *i, "is an unknown snomask character");
                    continue;
                } else if (!user->IsOper()) {
                    user->WriteNumeric(ERR_NOPRIVILEGES,
                                       InspIRCd::Format("Permission Denied - Only operators may %sset snomask %c",
                                                        adding ? "" : "un", *i));
                    continue;

                } else if (!user->HasSnomaskPermission(*i)) {
                    user->WriteNumeric(ERR_NOPRIVILEGES,
                                       InspIRCd::Format("Permission Denied - Oper type %s does not have access to snomask %c",
                                                        user->oper->name.c_str(), *i));
                    continue;
                }
            } else if (!(((*i >= 'a') && (*i <= 'z')) || ((*i >= 'A') && (*i <= 'Z')))) {
                continue;
            }

            size_t index = ((*i) - 'A');
            curr[index] = adding;
            break;
        }
    }

    std::string plus = "+";
    std::string minus = "-";

    // Apply changes and construct two strings consisting of the newly added and the removed snomask chars
    for (size_t i = 0; i < 64; i++) {
        bool isset = curr[i];
        if (user->snomasks[i] != isset) {
            user->snomasks[i] = isset;
            std::string& appendhere = (isset ? plus : minus);
            appendhere.push_back(i+'A');
        }
    }

    // Create the final string that will be shown to the user and sent to servers
    // Form: "+ABc-de"
    std::string output;
    if (plus.length() > 1) {
        output = plus;
    }

    if (minus.length() > 1) {
        output += minus;
    }

    // Unset the snomask usermode itself if every snomask was unset
    if (user->snomasks.none()) {
        user->SetMode(this, false);
    }

    return output;
}
