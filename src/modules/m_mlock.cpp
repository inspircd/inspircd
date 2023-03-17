/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2017 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Ariadne Conill <ariadne@dereferenced.org>
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

enum {
    // From Charybdis.
    ERR_MLOCKRESTRICTED = 742
};

class ModuleMLock : public Module {
    StringExtItem mlock;

  public:
    ModuleMLock()
        : mlock("mlock", ExtensionItem::EXT_CHANNEL, this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows services to lock channel modes so that they can not be changed.", VF_VENDOR);
    }

    ModResult OnRawMode(User* source, Channel* channel, ModeHandler* mh,
                        const std::string& parameter, bool adding) CXX11_OVERRIDE {
        if (!channel) {
            return MOD_RES_PASSTHRU;
        }

        if (!IS_LOCAL(source)) {
            return MOD_RES_PASSTHRU;
        }

        std::string *mlock_str = mlock.get(channel);
        if (!mlock_str) {
            return MOD_RES_PASSTHRU;
        }

        const char mode = mh->GetModeChar();
        std::string::size_type p = mlock_str->find(mode);
        if (p != std::string::npos) {
            source->WriteNumeric(ERR_MLOCKRESTRICTED, channel->name, mode, *mlock_str,
                                 "MODE cannot be set due to the channel having an active MLOCK restriction policy");
            return MOD_RES_DENY;
        }

        return MOD_RES_PASSTHRU;
    }
};

MODULE_INIT(ModuleMLock)
