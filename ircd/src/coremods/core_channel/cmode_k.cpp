/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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
#include "core_channel.h"

const std::string::size_type ModeChannelKey::maxkeylen = 32;

ModeChannelKey::ModeChannelKey(Module* Creator)
    : ParamMode<ModeChannelKey, LocalStringExt>(Creator, "key", 'k', PARAM_ALWAYS) {
    syntax = "<key>";
}

ModeAction ModeChannelKey::OnModeChange(User* source, User*, Channel* channel,
                                        std::string &parameter, bool adding) {
    const std::string* key = ext.get(channel);
    bool exists = (key != NULL);
    if (IS_LOCAL(source)) {
        if (exists == adding) {
            return MODEACTION_DENY;
        }
        if (exists && (parameter != *key)) {
            /* Key is currently set and the correct key wasn't given */
            source->WriteNumeric(ERR_KEYSET, channel->name, "Channel key already set");
            return MODEACTION_DENY;
        }
    } else {
        if (exists && adding && parameter == *key) {
            /* no-op, don't show */
            return MODEACTION_DENY;
        }
    }

    if (adding) {
        // When joining a channel multiple keys are delimited with a comma so we strip
        // them out here to avoid creating channels that are unjoinable.
        size_t commapos;
        while ((commapos = parameter.find(',')) != std::string::npos) {
            parameter.erase(commapos, 1);
        }

        // Truncate the parameter to the maximum key length.
        if (parameter.length() > maxkeylen) {
            parameter.erase(maxkeylen);
        }

        // If the password is empty here then it only consisted of commas. This is not
        // acceptable so we reject the mode change.
        if (parameter.empty()) {
            return MODEACTION_DENY;
        }

        ext.set(channel, parameter);
    } else {
        ext.unset(channel);
    }

    channel->SetMode(this, adding);
    return MODEACTION_ALLOW;
}

void ModeChannelKey::SerializeParam(Channel* chan, const std::string* key,
                                    std::string& out) {
    out += *key;
}

ModeAction ModeChannelKey::OnSet(User* source, Channel* chan,
                                 std::string& param) {
    // Dummy function, never called
    return MODEACTION_DENY;
}

bool ModeChannelKey::IsParameterSecret() {
    return true;
}
