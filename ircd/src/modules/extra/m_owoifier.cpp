/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Sadie Powell <sadie@witchery.services>
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


/// $CompilerFlags: -std=c++11

/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDesc: OwO, whats this?
/// $ModDepends: core 3

#include "inspircd.h"

#include <regex>

static const char FACES[6][16] = {
    "(・`ω´・) ",
    ";;w;; ",
    "owo ",
    "UwU ",
    ">w< ",
    "^w^ "
};

class ModuleOwoifier : public Module {
  public:
    ModResult OnUserPreMessage(User* user, const MessageTarget& target,
                               MessageDetails& details) override {
        if (!IS_LOCAL(user)) {
            return MOD_RES_PASSTHRU;
        }

        // Replace r and l with w.
        details.text = std::regex_replace(details.text, std::regex("[rl]"), "w");
        details.text = std::regex_replace(details.text, std::regex("[RL]"), "W");

        // Replace [Nn][vowel] with [N|n][Y|y][vowel].
        details.text = std::regex_replace(details.text, std::regex("n([aeiou])"),
                                          "ny$1");
        details.text = std::regex_replace(details.text, std::regex("n([AEIOU])"),
                                          "nY$1");
        details.text = std::regex_replace(details.text, std::regex("N([aeiou])"),
                                          "Ny$1");
        details.text = std::regex_replace(details.text, std::regex("N([AEIOU])"),
                                          "NY$1");

        // Replace ove with uv.
        details.text = std::regex_replace(details.text, std::regex("ove"), "uv");
        details.text = std::regex_replace(details.text, std::regex("Ove"), "Uv");
        details.text = std::regex_replace(details.text, std::regex("OVe"), "UV");
        details.text = std::regex_replace(details.text, std::regex("OVE"), "UV");

        // Replace the ! at the end of a sentence with a face.
        const char* face = FACES[ServerInstance->GenRandomInt(5)];
        details.text = std::regex_replace(details.text, std::regex("! ?"), face);

        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() override {
        return Version("OwO, whats this?", VF_OPTCOMMON);
    }
};

MODULE_INIT(ModuleOwoifier)

